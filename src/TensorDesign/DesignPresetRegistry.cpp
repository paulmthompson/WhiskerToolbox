/**
 * @file DesignPresetRegistry.cpp
 * @brief Built-in TensorDesign design authoring preset registry implementation.
 */

#include "DesignPresetRegistry.hpp"

#include "ColumnRecipePresetRegistry.hpp"

#include <nlohmann/json.hpp>

#include <algorithm>
#include <string>

namespace Neuralyzer::TensorDesign {

namespace {

[[nodiscard]] ParameterSchema whiskerContactFeatureTableSchema() {
    ParameterSchema schema;
    schema.params_type_name = "WhiskerContactFeatureTablePresetArgs";
    schema.fields.push_back(ParameterFieldDescriptor{
            .name = "row_source_key",
            .type_name = "data_key",
            .raw_type_name = "std::string",
            .display_name = "Row Source Key",
            .tooltip = "DigitalIntervalSeries key defining one table row per contact interval."});
    schema.fields.push_back(ParameterFieldDescriptor{
            .name = "curvature_source_key",
            .type_name = "data_key",
            .raw_type_name = "std::string",
            .display_name = "Curvature Source Key",
            .tooltip = "Analog-like source reduced with MeanValue over each contact interval."});
    schema.fields.push_back(ParameterFieldDescriptor{
            .name = "spike_source_key",
            .type_name = "data_key",
            .raw_type_name = "std::string",
            .display_name = "Spike Source Key",
            .tooltip = "DigitalEventSeries source counted over each contact interval and around contact onset."});
    schema.fields.push_back(ParameterFieldDescriptor{
            .name = "angle_source_key",
            .type_name = "data_key",
            .raw_type_name = "std::string",
            .display_name = "Angle Source Key",
            .tooltip = "Analog-like source sampled at contact onset."});
    schema.fields.push_back(ParameterFieldDescriptor{
            .name = "keypoint_source_keys",
            .type_name = "std::vector<std::string>",
            .raw_type_name = "std::vector<std::string>",
            .display_name = "Keypoint Source Keys",
            .tooltip = "Optional PointData keys sampled at contact onset and expanded into x/y columns."});
    schema.fields.push_back(ParameterFieldDescriptor{
            .name = "onset_pre",
            .type_name = "int",
            .raw_type_name = "int64_t",
            .display_name = "Onset Pre",
            .tooltip = "Clock ticks before contact onset for the spike-presence window."});
    schema.fields.push_back(ParameterFieldDescriptor{
            .name = "onset_post",
            .type_name = "int",
            .raw_type_name = "int64_t",
            .display_name = "Onset Post",
            .tooltip = "Clock ticks after contact onset for the spike-presence window."});
    return schema;
}

[[nodiscard]] std::optional<DesignPresetExpansion> expandWhiskerContactFeatureTable(
        DesignPresetArgs const & args) {
    if (args.row_source_key.empty() || args.curvature_source_key.empty() ||
        args.spike_source_key.empty() || args.angle_source_key.empty() || args.onset_pre < 0 ||
        args.onset_post < 0) {
        return std::nullopt;
    }

    auto row_registry = createBuiltInRowModifierRegistry();
    auto agg_registry = createBuiltInColumnAggregatorRegistry();
    DesignPresetExpansion expansion;
    expansion.spec.row_type = RowType::Interval;
    expansion.spec.row_source_key = args.row_source_key;

    auto stitch = [&](std::string const & mod_id, std::string const & agg_id, ColumnRecipePresetArgs const & agg_args) -> bool {
        auto agg_desc = agg_registry.find(agg_id);
        if (!agg_desc) return false;
        auto agg_exp = agg_desc->expand(agg_args);
        if (!agg_exp) return false;
        
        if (!mod_id.empty()) {
            auto mod_desc = row_registry.find(mod_id);
            if (!mod_desc) return false;
            auto mod_exp = mod_desc->expand(agg_args);
            if (!mod_exp) return false;
            for (auto & col : agg_exp->columns) {
                col.row_pipeline_json = mod_exp->row_pipeline_json;
            }
        }
        for (auto & col: agg_exp->columns) {
            expansion.spec.columns.push_back(std::move(col));
        }
        return true;
    };

    if (!stitch("", "mean_value", ColumnRecipePresetArgs{.output_name = "mean_curvature", .source_key = args.curvature_source_key})) return std::nullopt;
    if (!stitch("", "event_count", ColumnRecipePresetArgs{.output_name = "spike_count", .source_key = args.spike_source_key})) return std::nullopt;
    if (!stitch("window_around_interval_start", "event_presence", ColumnRecipePresetArgs{.output_name = "spike_presence_at_onset", .source_key = args.spike_source_key, .pre = args.onset_pre, .post = args.onset_post})) return std::nullopt;
    if (!stitch("interval_start", "analog_sample", ColumnRecipePresetArgs{.output_name = "angle_at_onset", .source_key = args.angle_source_key})) return std::nullopt;

    for (auto const & keypoint_source_key: args.keypoint_source_keys) {
        if (keypoint_source_key.empty()) {
            return std::nullopt;
        }
        if (!stitch("interval_start", "point_xy", ColumnRecipePresetArgs{.source_key = keypoint_source_key, .name_prefix = keypoint_source_key})) return std::nullopt;
    }

    return expansion;
}

[[nodiscard]] DesignPresetDescriptor whiskerContactFeatureTableDescriptor() {
    return DesignPresetDescriptor{
            .id = "whisker_contact_feature_table",
            .display_name = "Whisker contact feature table",
            .description = "Create a contact-interval feature table with curvature, spike count, onset spike presence, angle at onset, and optional keypoint x/y columns.",
            .parameters = whiskerContactFeatureTableSchema(),
            .source = DesignPresetSource::BuiltIn,
            .expand = expandWhiskerContactFeatureTable};
}

}// namespace

bool DesignPresetRegistry::registerPreset(DesignPresetDescriptor descriptor) {
    auto const duplicate = std::ranges::any_of(
            _descriptors,
            [&descriptor](DesignPresetDescriptor const & existing) {
                return existing.id == descriptor.id;
            });
    if (duplicate) {
        return false;
    }
    _descriptors.push_back(std::move(descriptor));
    return true;
}

DesignPresetDescriptor const * DesignPresetRegistry::find(std::string const & id) const {
    auto const iter = std::ranges::find_if(
            _descriptors,
            [&id](DesignPresetDescriptor const & descriptor) {
                return descriptor.id == id;
            });
    if (iter == _descriptors.end()) {
        return nullptr;
    }
    return &(*iter);
}

std::optional<DesignPresetExpansion> DesignPresetRegistry::expand(
        std::string const & id,
        DesignPresetArgs const & args) const {
    auto const * descriptor = find(id);
    if (descriptor == nullptr || !descriptor->expand) {
        return std::nullopt;
    }
    return descriptor->expand(args);
}

std::optional<DesignPresetExpansion> DesignPresetRegistry::expandJson(
        std::string const & id,
        nlohmann::json const & parameters) const {
    auto args = parseDesignPresetArgs(parameters);
    if (!args.has_value()) {
        return std::nullopt;
    }
    return expand(id, args.value());
}

std::vector<DesignPresetDescriptor const *> DesignPresetRegistry::descriptors() const {
    std::vector<DesignPresetDescriptor const *> result;
    result.reserve(_descriptors.size());
    for (auto const & descriptor: _descriptors) {
        result.push_back(&descriptor);
    }
    return result;
}

DesignPresetRegistry createBuiltInDesignPresetRegistry() {
    DesignPresetRegistry registry;
    static_cast<void>(registry.registerPreset(whiskerContactFeatureTableDescriptor()));
    return registry;
}

std::optional<DesignPresetArgs> parseDesignPresetArgs(nlohmann::json const & parameters) {
    if (!parameters.is_object()) {
        return std::nullopt;
    }

    DesignPresetArgs args;
    args.row_source_key = parameters.value("row_source_key", parameters.value("row_source", std::string{}));
    args.curvature_source_key = parameters.value("curvature_source_key", std::string{});
    args.spike_source_key = parameters.value("spike_source_key", std::string{});
    args.angle_source_key = parameters.value("angle_source_key", std::string{});
    args.onset_pre = parameters.value("onset_pre", int64_t{0});
    args.onset_post = parameters.value("onset_post", int64_t{0});

    if (parameters.contains("keypoint_source_keys")) {
        if (!parameters["keypoint_source_keys"].is_array()) {
            return std::nullopt;
        }
        for (auto const & source_key: parameters["keypoint_source_keys"]) {
            if (!source_key.is_string()) {
                return std::nullopt;
            }
            args.keypoint_source_keys.push_back(source_key.get<std::string>());
        }
    }

    return args;
}

}// namespace Neuralyzer::TensorDesign
