/**
 * @file TensorDesignBuilder.cpp
 * @brief Implementation of lazy-column tensor building from design JSON.
 */

#include "TensorDesignBuilder.hpp"

#include "DataManager/DataManager.hpp"
#include "DataManager/utils/TimeIndexExtractor.hpp"
#include "DigitalTimeSeries/Digital_Event_Series.hpp"
#include "DigitalTimeSeries/Digital_Interval_Series.hpp"
#include "Tensors/RowDescriptor.hpp"
#include "Tensors/TensorData.hpp"
#include "Tensors/storage/LazyColumnTensorStorage.hpp"
#include "TimeFrame/TimeIndexStorage.hpp"
#include "TimeFrame/interval_data.hpp"
#include "TransformsV2/core/TensorColumnBuilders.hpp"

#include <nlohmann/json.hpp>
#include <spdlog/spdlog.h>

#include <string>
#include <utility>
#include <vector>

namespace Neuralyzer::TensorDesign {

namespace {

using Neuralyzer::TensorBuilders::buildAnalogSampleAtOffsetProvider;
using Neuralyzer::TensorBuilders::buildIntervalPropertyProvider;
using Neuralyzer::TensorBuilders::buildInvalidationWiringFn;
using Neuralyzer::TensorBuilders::buildProviderFromRecipe;
using Neuralyzer::TensorBuilders::ColumnRecipe;
using Neuralyzer::TensorBuilders::IntervalProperty;

[[nodiscard]] std::optional<RowType> parseRowType(std::string const & row_type_str) {
    if (row_type_str == "interval") {
        return RowType::Interval;
    }
    if (row_type_str == "timestamp") {
        return RowType::Timestamp;
    }
    if (row_type_str == "ordinal") {
        return RowType::Ordinal;
    }
    if (row_type_str == "derived_from_source") {
        return RowType::DerivedFromSource;
    }
    if (row_type_str == "none") {
        return RowType::None;
    }
    return std::nullopt;
}

[[nodiscard]] char const * rowTypeToString(RowType row_type) {
    switch (row_type) {
        case RowType::Interval:
            return "interval";
        case RowType::Timestamp:
            return "timestamp";
        case RowType::Ordinal:
            return "ordinal";
        case RowType::DerivedFromSource:
            return "derived_from_source";
        case RowType::None:
            return "none";
    }
    return "none";
}

[[nodiscard]] std::optional<IntervalProperty> parseIntervalProperty(
        std::string const & prop) {
    if (prop == "start") {
        return IntervalProperty::Start;
    }
    if (prop == "end") {
        return IntervalProperty::End;
    }
    if (prop == "duration") {
        return IntervalProperty::Duration;
    }
    return std::nullopt;
}

[[nodiscard]] char const * intervalPropertyToString(IntervalProperty property) {
    switch (property) {
        case IntervalProperty::Start:
            return "start";
        case IntervalProperty::End:
            return "end";
        case IntervalProperty::Duration:
            return "duration";
    }
    return "start";
}

[[nodiscard]] std::optional<ColumnRecipe> parseColumnRecipe(nlohmann::json const & col) {
    ColumnRecipe recipe;
    recipe.column_name = col.value("name", "");
    recipe.source_key = col.value("source_key", "");
    recipe.pipeline_json = col.value("pipeline_json", "");

    if (col.contains("interval_property")) {
        auto const prop = col["interval_property"].get<std::string>();
        auto const parsed = parseIntervalProperty(prop);
        if (!parsed.has_value()) {
            spdlog::error(
                    "TensorDesign: unknown interval_property '{}'", prop);
            return std::nullopt;
        }
        recipe.interval_property = parsed;
    }

    return recipe;
}

struct RowBuildContext {
    std::shared_ptr<DigitalIntervalSeries> intervals;
    std::vector<TimeFrameIndex> row_times;
    RowDescriptor row_desc = RowDescriptor::ordinal(0);
    std::size_t num_rows = 0;
};

[[nodiscard]] std::optional<RowBuildContext> buildRowContext(
        DataManager & dm,
        TensorDesignSpec const & spec) {
    if (spec.row_type == RowType::None) {
        spdlog::error("TensorDesign: row_type is none");
        return std::nullopt;
    }

    if (spec.columns.empty()) {
        spdlog::error("TensorDesign: no columns configured");
        return std::nullopt;
    }

    RowBuildContext ctx;

    if (spec.row_type == RowType::Interval) {
        ctx.intervals = dm.getData<DigitalIntervalSeries>(spec.row_source_key);
        if (!ctx.intervals || ctx.intervals->size() == 0) {
            spdlog::error(
                    "TensorDesign: row source '{}' has no intervals",
                    spec.row_source_key);
            return std::nullopt;
        }
        ctx.num_rows = ctx.intervals->size();

        std::vector<TimeFrameInterval> tfi_intervals;
        tfi_intervals.reserve(ctx.num_rows);
        for (auto const & iw: ctx.intervals->view()) {
            tfi_intervals.push_back(TimeFrameInterval{
                    TimeFrameIndex(iw.interval.start),
                    TimeFrameIndex(iw.interval.end)});
        }
        ctx.row_desc = RowDescriptor::fromIntervals(
                std::move(tfi_intervals), ctx.intervals->getTimeFrame());
        return ctx;
    }

    if (spec.row_type == RowType::Timestamp) {
        auto events = dm.getData<DigitalEventSeries>(spec.row_source_key);
        if (!events || events->size() == 0) {
            spdlog::error(
                    "TensorDesign: row source '{}' has no events",
                    spec.row_source_key);
            return std::nullopt;
        }
        ctx.num_rows = events->size();
        ctx.row_times.reserve(ctx.num_rows);
        for (auto const & ew: events->view()) {
            ctx.row_times.push_back(ew.event_time);
        }

        auto time_storage = TimeIndexStorageFactory::createFromTimeIndices(ctx.row_times);
        ctx.row_desc = RowDescriptor::fromTimeIndices(
                std::move(time_storage), events->getTimeFrame());
        return ctx;
    }

    if (spec.row_type == RowType::Ordinal) {
        spdlog::error("TensorDesign: ordinal row type is not yet supported");
        return std::nullopt;
    }

    if (spec.row_type == RowType::DerivedFromSource) {
        auto result = extractTimeIndices(dm, spec.row_source_key);
        if (result.empty()) {
            spdlog::error(
                    "TensorDesign: row source '{}' has no timestamps",
                    spec.row_source_key);
            return std::nullopt;
        }
        ctx.row_times = std::move(result.indices);
        ctx.num_rows = ctx.row_times.size();

        auto time_storage = TimeIndexStorageFactory::createFromTimeIndices(ctx.row_times);
        ctx.row_desc = RowDescriptor::fromTimeIndices(
                std::move(time_storage), std::move(result.time_frame));
        return ctx;
    }

    spdlog::error("TensorDesign: unsupported row_type");
    return std::nullopt;
}

[[nodiscard]] ColumnProviderFn buildColumnProvider(
        DataManager & dm,
        ColumnRecipe const & recipe,
        std::vector<TimeFrameIndex> const & row_times,
        std::shared_ptr<DigitalIntervalSeries> const & intervals) {
    if (recipe.interval_property.has_value()) {
        return buildIntervalPropertyProvider(intervals, recipe.interval_property.value());
    }

    if (recipe.pipeline_json.find("\"offset\"") != std::string::npos) {
        try {
            auto const j = nlohmann::json::parse(recipe.pipeline_json);
            auto const offset = j.value("offset", int64_t{0});
            return buildAnalogSampleAtOffsetProvider(
                    dm, recipe.source_key, row_times, offset);
        } catch (...) {
            return buildProviderFromRecipe(dm, recipe, row_times, intervals);
        }
    }

    return buildProviderFromRecipe(dm, recipe, row_times, intervals);
}

}// namespace

std::optional<TensorDesignSpec> parseDesignJson(std::string const & json) {
    try {
        auto const j = nlohmann::json::parse(json);
        TensorDesignSpec spec;

        if (j.contains("tensor_key")) {
            spec.tensor_key = j["tensor_key"].get<std::string>();
        }
        if (j.contains("output_time_key")) {
            spec.output_time_key = j["output_time_key"].get<std::string>();
        }

        if (j.contains("row_source")) {
            auto const & rs = j["row_source"];
            std::string const row_type_str = rs.value("row_type", "none");
            auto const parsed_row_type = parseRowType(row_type_str);
            if (!parsed_row_type.has_value()) {
                spdlog::error(
                        "TensorDesign: unknown row_type '{}'", row_type_str);
                return std::nullopt;
            }
            spec.row_type = parsed_row_type.value();
            spec.row_source_key = rs.value("data_key", "");
        }

        if (j.contains("columns") && j["columns"].is_array()) {
            for (auto const & col: j["columns"]) {
                auto const recipe = parseColumnRecipe(col);
                if (!recipe.has_value()) {
                    return std::nullopt;
                }
                spec.columns.push_back(recipe.value());
            }
        }

        return spec;
    } catch (std::exception const & e) {
        spdlog::error("TensorDesign: failed to parse JSON: {}", e.what());
        return std::nullopt;
    }
}

std::string serializeDesignJson(TensorDesignSpec const & spec) {
    nlohmann::json j;

    if (!spec.tensor_key.empty()) {
        j["tensor_key"] = spec.tensor_key;
    }
    if (spec.output_time_key != "default") {
        j["output_time_key"] = spec.output_time_key;
    }

    nlohmann::json row_source;
    row_source["data_key"] = spec.row_source_key;
    row_source["row_type"] = rowTypeToString(spec.row_type);
    j["row_source"] = row_source;

    nlohmann::json columns = nlohmann::json::array();
    for (auto const & recipe: spec.columns) {
        nlohmann::json col;
        col["name"] = recipe.column_name;
        col["source_key"] = recipe.source_key;
        col["pipeline_json"] = recipe.pipeline_json;
        if (recipe.interval_property.has_value()) {
            col["interval_property"] =
                    intervalPropertyToString(recipe.interval_property.value());
        }
        columns.push_back(col);
    }
    j["columns"] = columns;

    return j.dump(2);
}

std::optional<TensorData> buildTensor(DataManager & dm, TensorDesignSpec const & spec) {
    auto row_ctx = buildRowContext(dm, spec);
    if (!row_ctx.has_value()) {
        return std::nullopt;
    }

    std::vector<ColumnSource> column_sources;
    std::vector<std::string> source_keys;
    column_sources.reserve(spec.columns.size());

    for (auto const & recipe: spec.columns) {
        auto provider = buildColumnProvider(
                dm, recipe, row_ctx->row_times, row_ctx->intervals);

        column_sources.push_back(ColumnSource{
                .name = recipe.column_name,
                .provider = std::move(provider)});

        if (!recipe.source_key.empty()) {
            source_keys.push_back(recipe.source_key);
        }
    }

    auto wiring = buildInvalidationWiringFn(dm, source_keys);

    try {
        return TensorData::createFromLazyColumns(
                row_ctx->num_rows,
                std::move(column_sources),
                std::move(row_ctx->row_desc),
                wiring);
    } catch (std::exception const & e) {
        spdlog::error("TensorDesign: failed to build tensor: {}", e.what());
        return std::nullopt;
    }
}

std::optional<TensorData> buildTensorFromDesignJson(
        DataManager & dm,
        std::string const & json) {
    auto const spec = parseDesignJson(json);
    if (!spec.has_value()) {
        return std::nullopt;
    }
    return buildTensor(dm, spec.value());
}

bool populateDataManager(DataManager & dm, TensorDesignSpec const & spec) {
    if (spec.tensor_key.empty()) {
        spdlog::error("TensorDesign: tensor_key is required to populate DataManager");
        return false;
    }

    auto tensor = buildTensor(dm, spec);
    if (!tensor.has_value()) {
        return false;
    }

    dm.setData<TensorData>(
            spec.tensor_key,
            std::make_shared<TensorData>(std::move(tensor.value())),
            TimeKey(spec.output_time_key));
    return true;
}

}// namespace Neuralyzer::TensorDesign
