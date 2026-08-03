/**
 * @file TensorDesignBuilder.cpp
 * @brief Implementation of lazy-column tensor building from design JSON.
 */

#include "TensorDesignBuilder.hpp"

#include "DataManager/DataManager.hpp"
#include "DataManager/utils/TimeIndexExtractor.hpp"
#include "DigitalTimeSeries/Digital_Event_Series.hpp"
#include "DigitalTimeSeries/Digital_Interval_Series.hpp"
#include "TensorDesign/ColumnRecipePresetRegistry.hpp"
#include "TensorDesign/DesignPresetRegistry.hpp"
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
    if (row_type_str == "timeframe") {
        return RowType::TimeFrame;
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
        case RowType::TimeFrame:
            return "timeframe";
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
    recipe.row_pipeline_json = col.value("row_pipeline_json", "");

    if (col.contains("pipeline_value_bindings")) {
        if (!col["pipeline_value_bindings"].is_array()) {
            spdlog::error("TensorDesign: pipeline_value_bindings must be an array");
            return std::nullopt;
        }
        for (auto const & binding_json: col["pipeline_value_bindings"]) {
            Neuralyzer::TensorBuilders::PipelineValueBindingRecipe binding;
            binding.source_key = binding_json.value("source_key", "");
            binding.source_pipeline_json = binding_json.value("source_pipeline_json", "");
            binding.store_key = binding_json.value("store_key", "");
            recipe.pipeline_value_bindings.push_back(std::move(binding));
        }
    }

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

[[nodiscard]] nlohmann::json columnRecipeToJson(ColumnRecipe const & recipe) {
    nlohmann::json col;
    col["name"] = recipe.column_name;
    col["source_key"] = recipe.source_key;
    col["pipeline_json"] = recipe.pipeline_json;
    if (!recipe.row_pipeline_json.empty()) {
        col["row_pipeline_json"] = recipe.row_pipeline_json;
    }
    if (!recipe.pipeline_value_bindings.empty()) {
        nlohmann::json bindings = nlohmann::json::array();
        for (auto const & binding: recipe.pipeline_value_bindings) {
            nlohmann::json binding_json;
            binding_json["source_key"] = binding.source_key;
            binding_json["store_key"] = binding.store_key;
            if (!binding.source_pipeline_json.empty()) {
                binding_json["source_pipeline_json"] = binding.source_pipeline_json;
            }
            bindings.push_back(binding_json);
        }
        col["pipeline_value_bindings"] = bindings;
    }
    if (recipe.interval_property.has_value()) {
        col["interval_property"] = intervalPropertyToString(recipe.interval_property.value());
    }
    return col;
}

[[nodiscard]] std::optional<nlohmann::json> expandPresetColumns(nlohmann::json design_json) {
    if (!design_json.contains("columns") || !design_json["columns"].is_array()) {
        return design_json;
    }

    auto registry = createBuiltInColumnRecipePresetRegistry();
    nlohmann::json expanded_columns = nlohmann::json::array();
    for (auto const & col: design_json["columns"]) {
        if (!col.contains("preset")) {
            expanded_columns.push_back(col);
            continue;
        }

        auto const preset_id = col.value("preset", std::string{});
        if (preset_id.empty()) {
            spdlog::error("TensorDesign: preset column is missing preset id");
            return std::nullopt;
        }

        auto const parameters = col.value("parameters", nlohmann::json::object());
        auto expansion = registry.expandJson(preset_id, parameters);
        if (!expansion.has_value()) {
            spdlog::error("TensorDesign: failed to expand column preset '{}'", preset_id);
            return std::nullopt;
        }

        auto const row_modifier_id = col.value("row_modifier", std::string{});
        if (!row_modifier_id.empty()) {
            auto row_registry = Neuralyzer::TensorDesign::createBuiltInRowModifierRegistry();
            auto const * mod_desc = row_registry.find(row_modifier_id);
            if (!mod_desc) {
                spdlog::error("TensorDesign: unknown row_modifier '{}'", row_modifier_id);
                return std::nullopt;
            }
            auto const args = Neuralyzer::TensorDesign::parseColumnRecipePresetArgs(parameters);
            if (!args) {
                spdlog::error("TensorDesign: failed to parse parameters for row_modifier '{}'", row_modifier_id);
                return std::nullopt;
            }
            auto mod_exp = mod_desc->expand(*args);
            if (!mod_exp) {
                spdlog::error("TensorDesign: failed to expand row_modifier '{}'", row_modifier_id);
                return std::nullopt;
            }
            for (auto & recipe: expansion->columns) {
                if (!mod_exp->row_pipeline_json.empty()) {
                    recipe.row_pipeline_json = mod_exp->row_pipeline_json;
                }
                for (auto const & binding: mod_exp->pipeline_value_bindings) {
                    recipe.pipeline_value_bindings.push_back(binding);
                }
            }
        }

        for (auto const & recipe: expansion->columns) {
            expanded_columns.push_back(columnRecipeToJson(recipe));
        }
    }

    design_json["columns"] = std::move(expanded_columns);
    return design_json;
}

[[nodiscard]] std::optional<nlohmann::json> expandDesignPreset(nlohmann::json design_json) {
    if (!design_json.contains("preset")) {
        return design_json;
    }

    auto const preset_id = design_json.value("preset", std::string{});
    if (preset_id.empty()) {
        spdlog::error("TensorDesign: design preset is missing preset id");
        return std::nullopt;
    }

    auto registry = createBuiltInDesignPresetRegistry();
    auto const parameters = design_json.value("parameters", nlohmann::json::object());
    auto expansion = registry.expandJson(preset_id, parameters);
    if (!expansion.has_value()) {
        spdlog::error("TensorDesign: failed to expand design preset '{}'", preset_id);
        return std::nullopt;
    }

    nlohmann::json expanded_json = nlohmann::json::parse(serializeDesignJson(expansion->spec));
    if (design_json.contains("tensor_key")) {
        expanded_json["tensor_key"] = design_json["tensor_key"];
    }
    if (design_json.contains("output_time_key")) {
        expanded_json["output_time_key"] = design_json["output_time_key"];
    }
    return expanded_json;
}

struct RowBuildContext {
    std::shared_ptr<DigitalIntervalSeries const> intervals;
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
        for (std::size_t i = 0; i < ctx.num_rows; ++i) {
            tfi_intervals.push_back(ctx.intervals->getStoredInterval(i));
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
        for (std::size_t i = 0; i < ctx.num_rows; ++i) {
            ctx.row_times.push_back(events->getStoredEvent(i));
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

    if (spec.row_type == RowType::TimeFrame) {
        auto time_frame = dm.getTime(TimeKey(spec.row_time_key));
        if (!time_frame || time_frame->getTotalFrameCount() <= 0) {
            spdlog::error(
                    "TensorDesign: row TimeFrame '{}' is empty or not found",
                    spec.row_time_key);
            return std::nullopt;
        }

        auto const count = static_cast<std::size_t>(time_frame->getTotalFrameCount());
        ctx.row_times.reserve(count);
        for (std::size_t i = 0; i < count; ++i) {
            ctx.row_times.emplace_back(static_cast<int64_t>(i));
        }
        ctx.num_rows = ctx.row_times.size();

        auto time_storage = TimeIndexStorageFactory::createFromTimeIndices(ctx.row_times);
        ctx.row_desc = RowDescriptor::fromTimeIndices(
                std::move(time_storage), std::move(time_frame));
        return ctx;
    }

    spdlog::error("TensorDesign: unsupported row_type");
    return std::nullopt;
}

[[nodiscard]] ColumnProviderFn buildColumnProvider(
        DataManager & dm,
        ColumnRecipe const & recipe,
        std::vector<TimeFrameIndex> const & row_times,
        std::shared_ptr<DigitalIntervalSeries const> const & intervals) {
    if (recipe.interval_property.has_value()) {
        return buildIntervalPropertyProvider(intervals, recipe.interval_property.value());
    }

    return buildProviderFromRecipe(dm, recipe, row_times, intervals);
}

}// namespace

std::optional<TensorDesignSpec> parseDesignJson(std::string const & json) {
    try {
        auto parsed_json = nlohmann::json::parse(json);
        auto design_expanded_json = expandDesignPreset(std::move(parsed_json));
        if (!design_expanded_json.has_value()) {
            return std::nullopt;
        }
        auto const expanded_json = expandPresetColumns(std::move(design_expanded_json.value()));
        if (!expanded_json.has_value()) {
            return std::nullopt;
        }
        auto const & j = expanded_json.value();
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
            spec.row_time_key = rs.value("time_key", "");
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
    if (!spec.row_source_key.empty()) {
        row_source["data_key"] = spec.row_source_key;
    }
    if (!spec.row_time_key.empty()) {
        row_source["time_key"] = spec.row_time_key;
    }
    row_source["row_type"] = rowTypeToString(spec.row_type);
    j["row_source"] = row_source;

    nlohmann::json columns = nlohmann::json::array();
    for (auto const & recipe: spec.columns) {
        columns.push_back(columnRecipeToJson(recipe));
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

namespace {

[[nodiscard]] std::optional<TimeKey> tryRegisteredTimeKey(
        DataManager & dm,
        std::string const & key_str) {
    if (key_str.empty()) {
        return std::nullopt;
    }
    TimeKey candidate(key_str);
    if (dm.getTime(candidate) != nullptr) {
        return candidate;
    }
    return std::nullopt;
}

}// namespace

std::optional<TimeKey> resolveOutputTimeKey(
        DataManager & dm,
        TensorDesignSpec const & spec) {
    if (!spec.output_time_key.empty() && spec.output_time_key != "default") {
        if (auto const explicit_key = tryRegisteredTimeKey(dm, spec.output_time_key)) {
            return explicit_key;
        }
    }

    if (spec.row_type == RowType::TimeFrame) {
        if (auto const row_time_key = tryRegisteredTimeKey(dm, spec.row_time_key)) {
            return row_time_key;
        }
    } else if (spec.row_type != RowType::None && spec.row_type != RowType::Ordinal &&
               !spec.row_source_key.empty()) {
        auto const row_source_time_key = dm.getTimeKey(spec.row_source_key);
        if (!row_source_time_key.empty()) {
            if (auto const derived_key = tryRegisteredTimeKey(dm, row_source_time_key.str())) {
                return derived_key;
            }
        }
    }

    if (auto const time_key = tryRegisteredTimeKey(dm, "time")) {
        return time_key;
    }
    return tryRegisteredTimeKey(dm, "default");
}

bool populateDataManager(DataManager & dm, TensorDesignSpec const & spec) {
    if (spec.tensor_key.empty()) {
        spdlog::error("TensorDesign: tensor_key is required to populate DataManager");
        return false;
    }

    auto const output_time_key = resolveOutputTimeKey(dm, spec);
    if (!output_time_key.has_value()) {
        spdlog::error(
                "TensorDesign: could not resolve output TimeKey for tensor '{}'",
                spec.tensor_key);
        return false;
    }

    auto tensor = buildTensor(dm, spec);
    if (!tensor.has_value()) {
        return false;
    }

    dm.setData<TensorData>(
            spec.tensor_key,
            std::make_shared<TensorData>(std::move(tensor.value())),
            output_time_key.value());
    return true;
}

}// namespace Neuralyzer::TensorDesign
