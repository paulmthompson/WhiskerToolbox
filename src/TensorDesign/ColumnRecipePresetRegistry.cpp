/**
 * @file ColumnRecipePresetRegistry.cpp
 * @brief Built-in TensorDesign column recipe preset registry implementation.
 */

#include "ColumnRecipePresetRegistry.hpp"

#include <nlohmann/json.hpp>

#include <algorithm>
#include <string>

namespace Neuralyzer::TensorDesign {

namespace {

[[nodiscard]] ParameterSchema meanOverIntervalSchema() {
    ParameterSchema schema;
    schema.params_type_name = "MeanOverIntervalPresetArgs";
    schema.fields.push_back(ParameterFieldDescriptor{
            .name = "output_name",
            .type_name = "std::string",
            .raw_type_name = "std::string",
            .display_name = "Output Name",
            .tooltip = "Name of the generated tensor column."});
    schema.fields.push_back(ParameterFieldDescriptor{
            .name = "source_key",
            .type_name = "data_key",
            .raw_type_name = "std::string",
            .display_name = "Source Key",
            .tooltip = "Analog-like data source reduced over each row interval."});
    return schema;
}

[[nodiscard]] ParameterSchema outputSourceSchema(std::string params_type_name) {
    auto schema = meanOverIntervalSchema();
    schema.params_type_name = std::move(params_type_name);
    return schema;
}

[[nodiscard]] ParameterSchema eventWindowSchema() {
    auto schema = outputSourceSchema("EventPresenceAroundIntervalStartPresetArgs");
    schema.fields.push_back(ParameterFieldDescriptor{
            .name = "pre",
            .type_name = "int",
            .raw_type_name = "int64_t",
            .display_name = "Pre",
            .tooltip = "Clock ticks before the row interval start."});
    schema.fields.push_back(ParameterFieldDescriptor{
            .name = "post",
            .type_name = "int",
            .raw_type_name = "int64_t",
            .display_name = "Post",
            .tooltip = "Clock ticks after the row interval start."});
    return schema;
}

[[nodiscard]] ParameterSchema rowModifierWindowSchema() {
    ParameterSchema schema;
    schema.params_type_name = "RowModifierWindowArgs";
    schema.fields.push_back(ParameterFieldDescriptor{
            .name = "pre",
            .type_name = "int",
            .raw_type_name = "int64_t",
            .display_name = "Pre",
            .tooltip = "Clock ticks before the event."});
    schema.fields.push_back(ParameterFieldDescriptor{
            .name = "post",
            .type_name = "int",
            .raw_type_name = "int64_t",
            .display_name = "Post",
            .tooltip = "Clock ticks after the event."});
    return schema;
}

[[nodiscard]] ParameterSchema pointXySchema() {
    ParameterSchema schema;
    schema.params_type_name = "PointXyPresetArgs";
    schema.fields.push_back(ParameterFieldDescriptor{
            .name = "source_key",
            .type_name = "data_key",
            .raw_type_name = "std::string",
            .display_name = "Source Key",
            .tooltip = "PointData key to expand into x and y columns."});
    schema.fields.push_back(ParameterFieldDescriptor{
            .name = "name_prefix",
            .type_name = "std::string",
            .raw_type_name = "std::string",
            .display_name = "Name Prefix",
            .tooltip = "Prefix for generated x and y column names."});
    return schema;
}

[[nodiscard]] ParameterSchema multiPointXySchema() {
    ParameterSchema schema;
    schema.params_type_name = "MultiPointXyPresetArgs";
    schema.fields.push_back(ParameterFieldDescriptor{
            .name = "source_keys",
            .type_name = "std::vector<std::string>",
            .raw_type_name = "std::vector<std::string>",
            .display_name = "Source Keys",
            .tooltip = "PointData keys to expand into x and y columns."});
    return schema;
}

[[nodiscard]] ParameterSchema trialRelativeEventSchema(std::string params_type_name) {
    auto schema = outputSourceSchema(std::move(params_type_name));
    schema.fields.push_back(ParameterFieldDescriptor{
            .name = "binding_source_key",
            .type_name = "data_key",
            .raw_type_name = "std::string",
            .display_name = "Binding Source Key",
            .tooltip = "Interval source used to derive one interval-start alignment event per row."});
    schema.fields.push_back(ParameterFieldDescriptor{
            .name = "store_key",
            .type_name = "std::string",
            .raw_type_name = "std::string",
            .display_name = "Store Key",
            .tooltip = "PipelineValueStore key used for row alignment time."});
    schema.fields.push_back(ParameterFieldDescriptor{
            .name = "window_start",
            .type_name = "double",
            .raw_type_name = "double",
            .display_name = "Window Start",
            .tooltip = "Relative window start for event count reductions."});
    schema.fields.push_back(ParameterFieldDescriptor{
            .name = "window_end",
            .type_name = "double",
            .raw_type_name = "double",
            .display_name = "Window End",
            .tooltip = "Relative window end for event count reductions."});
    return schema;
}

[[nodiscard]] std::string intervalStartPipelineJson() {
    return R"({"steps": [{"step_id": "interval_start", "transform_name": "IntervalToEvent", "parameters": {"point": "start"}}]})";
}

// DELETED

// DELETED

[[nodiscard]] TensorBuilders::ColumnRecipe pointCoordinateRecipe(
        std::string column_name,
        std::string source_key,
        char const * step_id,
        char const * coordinate,
        std::string row_pipeline_json = {},
        bool reduce_mean = false) {
    TensorBuilders::ColumnRecipe recipe;
    recipe.column_name = std::move(column_name);
    recipe.source_key = std::move(source_key);
    recipe.row_pipeline_json = std::move(row_pipeline_json);
    recipe.pipeline_json = R"({"steps": [{"step_id": ")" + std::string(step_id) +
                           R"(", "transform_name": "PointCoordinate", "parameters": {"coordinate": ")" +
                           coordinate + R"("}}])";
    if (reduce_mean) {
        recipe.pipeline_json += R"(, "range_reduction": {"reduction_name": "MeanValue"})";
    }
    recipe.pipeline_json += "}";
    return recipe;
}

// DELETED

// DELETED

[[nodiscard]] std::string bindingStoreKey(ColumnRecipePresetArgs const & args) {
    if (!args.store_key.empty()) {
        return args.store_key;
    }
    return "row_alignment_time";
}

[[nodiscard]] std::string bindingSourceKey(ColumnRecipePresetArgs const & args) {
    if (!args.binding_source_key.empty()) {
        return args.binding_source_key;
    }
    return args.source_key;
}

[[nodiscard]] Neuralyzer::TensorBuilders::PipelineValueBindingRecipe intervalStartBinding(
        ColumnRecipePresetArgs const & args) {
    return Neuralyzer::TensorBuilders::PipelineValueBindingRecipe{
            .source_key = bindingSourceKey(args),
            .source_pipeline_json = intervalStartPipelineJson(),
            .store_key = bindingStoreKey(args)};
}

[[nodiscard]] std::string normalizeEventsRelativePipelineJson(std::string_view store_key) {
    return R"({"steps": [{"step_id": "normalize", "transform_name": "NormalizeDigitalEventSeriesRelative", "parameters": {"alignment_time": 0}, "param_bindings": {"alignment_time": ")" +
           std::string(store_key) + R"("}}]})";
}

[[nodiscard]] bool modifierIdInList(
        RowModifierDescriptor const * modifier,
        std::vector<std::string> const & compatible_modifier_ids) {
    if (compatible_modifier_ids.empty()) {
        return true;
    }
    auto const modifier_id = modifier != nullptr ? modifier->id : std::string{};
    return std::ranges::find(compatible_modifier_ids, modifier_id) != compatible_modifier_ids.end();
}

}// namespace


[[nodiscard]] std::optional<RowModifierExpansion> expandIntervalStartModifier(
        ColumnRecipePresetArgs const & /*args*/) {
    return RowModifierExpansion{.row_pipeline_json = intervalStartPipelineJson()};
}

[[nodiscard]] RowModifierDescriptor intervalStartModifierDescriptor() {
    return RowModifierDescriptor{
            .id = "interval_start",
            .display_name = "At interval start",
            .description = "Convert an interval to an event at its start.",
            .output_row_type = EffectiveRowType::Timestamp,
            .supported_row_types = {EffectiveRowType::Interval},
            .parameters = ParameterSchema{.params_type_name = "IntervalStartArgs"},
            .source = ColumnRecipePresetSource::BuiltIn,
            .expand = expandIntervalStartModifier};
}

[[nodiscard]] std::optional<RowModifierExpansion> expandWindowAroundIntervalStartModifier(
        ColumnRecipePresetArgs const & args) {
    if (args.pre < 0 || args.post < 0) {
        return std::nullopt;
    }
    return RowModifierExpansion{
            .row_pipeline_json = R"({"steps": [{"step_id": "interval_start", "transform_name": "IntervalToEvent", "parameters": {"point": "start"}}, {"step_id": "window", "transform_name": "EventToInterval", "parameters": {"pre_expansion": )" +
                                 std::to_string(args.pre) + R"(, "post_expansion": )" + std::to_string(args.post) + R"(}}]})"};
}

[[nodiscard]] RowModifierDescriptor windowAroundIntervalStartModifierDescriptor() {
    return RowModifierDescriptor{
            .id = "window_around_interval_start",
            .display_name = "Window around interval start",
            .description = "Create a window around the interval start.",
            .output_row_type = EffectiveRowType::Interval,
            .supported_row_types = {EffectiveRowType::Interval},
            .parameters = rowModifierWindowSchema(),
            .source = ColumnRecipePresetSource::BuiltIn,
            .expand = expandWindowAroundIntervalStartModifier};
}

[[nodiscard]] std::optional<RowModifierExpansion> expandBindIntervalStartModifier(
        ColumnRecipePresetArgs const & args) {
    RowModifierExpansion exp;
    exp.pipeline_value_bindings.push_back(intervalStartBinding(args));
    return exp;
}

[[nodiscard]] RowModifierDescriptor bindIntervalStartModifierDescriptor() {
    return RowModifierDescriptor{
            .id = "bind_interval_start",
            .display_name = "Bind interval start",
            .description = "Bind the interval start time to a variable for use in the column pipeline, without modifying the row shape.",
            .output_row_type = EffectiveRowType::Unchanged,
            .supported_row_types = {EffectiveRowType::Interval},
            .parameters = ParameterSchema{.params_type_name = "BindIntervalStartArgs",
                                          .fields = {
                                                  ParameterFieldDescriptor{
                                                          .name = "binding_source_key",
                                                          .type_name = "data_key",
                                                          .raw_type_name = "std::string",
                                                          .display_name = "Binding Source Key",
                                                          .tooltip = "Interval source to extract the start time from."},
                                                  ParameterFieldDescriptor{
                                                          .name = "store_key",
                                                          .type_name = "std::string",
                                                          .raw_type_name = "std::string",
                                                          .display_name = "Store Key",
                                                          .tooltip = "PipelineValueStore key used for row alignment time."}}},
            .source = ColumnRecipePresetSource::BuiltIn,
            .expand = expandBindIntervalStartModifier};
}

// --- Phase 9e: Column Aggregators ---

[[nodiscard]] std::optional<ColumnAggregatorExpansion> expandMeanValueAggregator(
        ColumnRecipePresetArgs const & args) {
    if (args.output_name.empty() || args.source_key.empty()) return std::nullopt;
    TensorBuilders::ColumnRecipe recipe;
    recipe.column_name = args.output_name;
    recipe.source_key = args.source_key;
    recipe.pipeline_json = R"({"steps": [], "range_reduction": {"reduction_name": "MeanValue"}})";
    return ColumnAggregatorExpansion{.columns = {std::move(recipe)}};
}

[[nodiscard]] ColumnAggregatorDescriptor meanValueAggregatorDescriptor() {
    return ColumnAggregatorDescriptor{
            .id = "mean_value",
            .display_name = "Mean Value",
            .description = "Calculate the mean of an analog signal over the row interval.",
            .supported_row_types = {EffectiveRowType::Interval},
            .composition_rules = {
                    .output_kind = ColumnOutputKind::ScalarFloat,
                    .required_row_geometry = EffectiveRowType::Interval,
                    .compatible_modifier_ids = {"", "window_around_interval_start"}},
            .parameters = outputSourceSchema("MeanValueArgs"),
            .source = ColumnRecipePresetSource::BuiltIn,
            .expand = expandMeanValueAggregator};
}

[[nodiscard]] std::optional<ColumnAggregatorExpansion> expandEventCountAggregator(
        ColumnRecipePresetArgs const & args) {
    if (args.output_name.empty() || args.source_key.empty()) return std::nullopt;
    TensorBuilders::ColumnRecipe recipe;
    recipe.column_name = args.output_name;
    recipe.source_key = args.source_key;
    recipe.pipeline_json = R"({"steps": [], "range_reduction": {"reduction_name": "EventCount"}})";
    return ColumnAggregatorExpansion{.columns = {std::move(recipe)}};
}

[[nodiscard]] ColumnAggregatorDescriptor eventCountAggregatorDescriptor() {
    return ColumnAggregatorDescriptor{
            .id = "event_count",
            .display_name = "Event Count",
            .description = "Count the total number of events within the row interval.",
            .supported_row_types = {EffectiveRowType::Interval},
            .composition_rules = {
                    .output_kind = ColumnOutputKind::ScalarFloat,
                    .required_row_geometry = EffectiveRowType::Interval,
                    .compatible_modifier_ids = {"", "window_around_interval_start"}},
            .parameters = outputSourceSchema("EventCountArgs"),
            .source = ColumnRecipePresetSource::BuiltIn,
            .expand = expandEventCountAggregator};
}

[[nodiscard]] std::optional<ColumnAggregatorExpansion> expandEventPresenceAggregator(
        ColumnRecipePresetArgs const & args) {
    if (args.output_name.empty() || args.source_key.empty()) return std::nullopt;
    TensorBuilders::ColumnRecipe recipe;
    recipe.column_name = args.output_name;
    recipe.source_key = args.source_key;
    recipe.pipeline_json = R"({"steps": [], "range_reduction": {"reduction_name": "EventPresence"}})";
    return ColumnAggregatorExpansion{.columns = {std::move(recipe)}};
}

[[nodiscard]] ColumnAggregatorDescriptor eventPresenceAggregatorDescriptor() {
    return ColumnAggregatorDescriptor{
            .id = "event_presence",
            .display_name = "Event Presence",
            .description = "Report whether any event is present within the row interval.",
            .supported_row_types = {EffectiveRowType::Interval},
            .composition_rules = {
                    .output_kind = ColumnOutputKind::ScalarFloat,
                    .required_row_geometry = EffectiveRowType::Interval,
                    .compatible_modifier_ids = {"", "window_around_interval_start"}},
            .parameters = outputSourceSchema("EventPresenceArgs"),
            .source = ColumnRecipePresetSource::BuiltIn,
            .expand = expandEventPresenceAggregator};
}

[[nodiscard]] std::optional<ColumnAggregatorExpansion> expandAnalogSampleAggregator(
        ColumnRecipePresetArgs const & args) {
    if (args.output_name.empty() || args.source_key.empty()) return std::nullopt;
    TensorBuilders::ColumnRecipe recipe;
    recipe.column_name = args.output_name;
    recipe.source_key = args.source_key;
    recipe.pipeline_json = R"({"steps": []})";
    return ColumnAggregatorExpansion{.columns = {std::move(recipe)}};
}

[[nodiscard]] ColumnAggregatorDescriptor analogSampleAggregatorDescriptor() {
    return ColumnAggregatorDescriptor{
            .id = "analog_sample",
            .display_name = "Analog Sample",
            .description = "Sample an analog signal at a specific timestamp.",
            .supported_row_types = {EffectiveRowType::Timestamp},
            .composition_rules = {
                    .output_kind = ColumnOutputKind::ScalarFloat,
                    .required_row_geometry = EffectiveRowType::Timestamp,
                    .compatible_modifier_ids = {"interval_start"}},
            .parameters = outputSourceSchema("AnalogSampleArgs"),
            .source = ColumnRecipePresetSource::BuiltIn,
            .expand = expandAnalogSampleAggregator};
}

[[nodiscard]] std::optional<ColumnAggregatorExpansion> expandPointXyAggregator(
        ColumnRecipePresetArgs const & args) {
    if (args.source_key.empty() || args.name_prefix.empty()) return std::nullopt;
    return ColumnAggregatorExpansion{.columns = {
                                             pointCoordinateRecipe(args.name_prefix + "_x", args.source_key, "x", "X"),
                                             pointCoordinateRecipe(args.name_prefix + "_y", args.source_key, "y", "Y")}};
}

[[nodiscard]] ColumnAggregatorDescriptor pointXyAggregatorDescriptor() {
    return ColumnAggregatorDescriptor{
            .id = "point_xy",
            .display_name = "Point XY",
            .description = "Expand a PointData keypoint into x and y columns at the row timestamp.",
            .supported_row_types = {EffectiveRowType::Timestamp},
            .composition_rules = {
                    .output_kind = ColumnOutputKind::ScalarFloat,
                    .required_row_geometry = EffectiveRowType::Timestamp,
                    .compatible_modifier_ids = {"interval_start"}},
            .parameters = pointXySchema(),
            .source = ColumnRecipePresetSource::BuiltIn,
            .expand = expandPointXyAggregator};
}

[[nodiscard]] std::optional<ColumnAggregatorExpansion> expandMultiPointXyAggregator(
        ColumnRecipePresetArgs const & args) {
    if (args.source_keys.empty()) return std::nullopt;
    ColumnAggregatorExpansion expansion;
    expansion.columns.reserve(args.source_keys.size() * 2);
    for (auto const & source_key: args.source_keys) {
        if (source_key.empty()) return std::nullopt;
        expansion.columns.push_back(pointCoordinateRecipe(source_key + "_x", source_key, "x", "X"));
        expansion.columns.push_back(pointCoordinateRecipe(source_key + "_y", source_key, "y", "Y"));
    }
    return expansion;
}

[[nodiscard]] ColumnAggregatorDescriptor multiPointXyAggregatorDescriptor() {
    return ColumnAggregatorDescriptor{
            .id = "multi_point_xy",
            .display_name = "Multi-Point XY",
            .description = "Expand multiple PointData keypoints into x and y columns at the row timestamp.",
            .supported_row_types = {EffectiveRowType::Timestamp},
            .composition_rules = {
                    .output_kind = ColumnOutputKind::ScalarFloat,
                    .required_row_geometry = EffectiveRowType::Timestamp,
                    .compatible_modifier_ids = {"interval_start"}},
            .parameters = multiPointXySchema(),
            .source = ColumnRecipePresetSource::BuiltIn,
            .expand = expandMultiPointXyAggregator};
}

[[nodiscard]] std::optional<ColumnAggregatorExpansion> expandRasterEventsRelativeAggregator(
        ColumnRecipePresetArgs const & args) {
    if (args.output_name.empty() || args.source_key.empty() || bindingSourceKey(args).empty()) {
        return std::nullopt;
    }
    TensorBuilders::ColumnRecipe recipe;
    recipe.column_name = args.output_name;
    recipe.source_key = args.source_key;
    recipe.pipeline_json = normalizeEventsRelativePipelineJson(args.store_key);
    return ColumnAggregatorExpansion{.columns = {std::move(recipe)}};
}

[[nodiscard]] ColumnAggregatorDescriptor rasterEventsRelativeAggregatorDescriptor() {
    return ColumnAggregatorDescriptor{
            .id = "raster_events_relative",
            .display_name = "Raster Events (Relative)",
            .description = "Gather events over each row interval and normalize to alignment time. "
                           "Intermediate gather transform; use trial_relative_event_count for a scalar tensor column.",
            .supported_row_types = {EffectiveRowType::Interval},
            .composition_rules = {
                    .output_kind = ColumnOutputKind::GatheredDataObject,
                    .gathered_output_type = GatheredOutputType::DigitalEventSeries,
                    .requires_pipeline_value_bindings = true,
                    .required_row_geometry = EffectiveRowType::Interval,
                    .compatible_modifier_ids = {"bind_interval_start"}},
            .parameters = trialRelativeEventSchema("RasterEventsRelativeAggregatorArgs"),
            .source = ColumnRecipePresetSource::BuiltIn,
            .expand = expandRasterEventsRelativeAggregator};
}

[[nodiscard]] std::optional<ColumnAggregatorExpansion> expandTrialRelativeEventCountAggregator(
        ColumnRecipePresetArgs const & args) {
    if (args.output_name.empty() || args.source_key.empty() || bindingSourceKey(args).empty() ||
        args.window_end < args.window_start) {
        return std::nullopt;
    }
    TensorBuilders::ColumnRecipe recipe;
    recipe.column_name = args.output_name;
    recipe.source_key = args.source_key;
    recipe.pipeline_json = normalizeEventsRelativePipelineJson(args.store_key);
    recipe.pipeline_json.erase(recipe.pipeline_json.size() - 1);
    recipe.pipeline_json += R"(, "range_reduction": {"reduction_name": "EventCountInWindow", "parameters": {"window_start": )" +
                            std::to_string(args.window_start) + R"(, "window_end": )" + std::to_string(args.window_end) +
                            R"(}}})";
    return ColumnAggregatorExpansion{.columns = {std::move(recipe)}};
}

[[nodiscard]] ColumnAggregatorDescriptor trialRelativeEventCountAggregatorDescriptor() {
    return ColumnAggregatorDescriptor{
            .id = "trial_relative_event_count",
            .display_name = "Trial Relative Event Count",
            .description = "Count events in a relative window aligned to a bound timestamp variable.",
            .supported_row_types = {EffectiveRowType::Interval},
            .composition_rules = {
                    .output_kind = ColumnOutputKind::ScalarFloat,
                    .requires_pipeline_value_bindings = true,
                    .required_row_geometry = EffectiveRowType::Interval,
                    .compatible_modifier_ids = {"bind_interval_start"}},
            .parameters = trialRelativeEventSchema("TrialRelativeEventCountAggregatorArgs"),
            .source = ColumnRecipePresetSource::BuiltIn,
            .expand = expandTrialRelativeEventCountAggregator};
}

bool ColumnRecipePresetRegistry::registerPreset(ColumnRecipePresetDescriptor descriptor) {
    auto const duplicate = std::ranges::any_of(
            _descriptors,
            [&descriptor](ColumnRecipePresetDescriptor const & existing) {
                return existing.id == descriptor.id;
            });
    if (duplicate) {
        return false;
    }
    _descriptors.push_back(std::move(descriptor));
    return true;
}

ColumnRecipePresetDescriptor const * ColumnRecipePresetRegistry::find(std::string const & id) const {
    auto const iter = std::ranges::find_if(
            _descriptors,
            [&id](ColumnRecipePresetDescriptor const & descriptor) {
                return descriptor.id == id;
            });
    if (iter == _descriptors.end()) {
        return nullptr;
    }
    return &(*iter);
}

std::optional<ColumnRecipePresetExpansion> ColumnRecipePresetRegistry::expand(
        std::string const & id,
        ColumnRecipePresetArgs const & args) const {
    auto const * descriptor = find(id);
    if (descriptor == nullptr || !descriptor->expand) {
        return std::nullopt;
    }
    return descriptor->expand(args);
}

std::optional<ColumnRecipePresetExpansion> ColumnRecipePresetRegistry::expandJson(
        std::string const & id,
        nlohmann::json const & parameters) const {
    auto args = parseColumnRecipePresetArgs(parameters);
    if (!args.has_value()) {
        return std::nullopt;
    }
    return expand(id, args.value());
}

std::vector<ColumnRecipePresetDescriptor const *> ColumnRecipePresetRegistry::descriptors() const {
    std::vector<ColumnRecipePresetDescriptor const *> result;
    result.reserve(_descriptors.size());
    for (auto const & descriptor: _descriptors) {
        result.push_back(&descriptor);
    }
    return result;
}

ColumnRecipePresetDescriptor wrapAggregator(ColumnAggregatorDescriptor const & agg) {
    return ColumnRecipePresetDescriptor{
            .id = agg.id,
            .display_name = agg.display_name,
            .description = agg.description,
            .parameters = agg.parameters,
            .source = agg.source,
            .expand = [expand_fn = agg.expand](ColumnRecipePresetArgs const & args) -> std::optional<ColumnRecipePresetExpansion> {
                auto exp = expand_fn(args);
                if (!exp) return std::nullopt;
                ColumnRecipePresetExpansion res;
                res.columns = std::move(exp->columns);
                return res;
            }};
}

ColumnRecipePresetRegistry createBuiltInColumnRecipePresetRegistry() {
    ColumnRecipePresetRegistry registry;

    // Phase 9e: Expose scalar ColumnAggregators as tensor presets (empty row_pipeline_json).
    auto agg_registry = createBuiltInColumnAggregatorRegistry();
    for (auto const * agg: agg_registry.descriptors()) {
        if (agg->composition_rules.output_kind != ColumnOutputKind::ScalarFloat) {
            continue;
        }
        static_cast<void>(registry.registerPreset(wrapAggregator(*agg)));
    }

    return registry;
}

std::optional<ColumnRecipePresetArgs> parseColumnRecipePresetArgs(nlohmann::json const & parameters) {
    if (!parameters.is_object()) {
        return std::nullopt;
    }

    ColumnRecipePresetArgs args;
    args.output_name = parameters.value("output_name", parameters.value("name", std::string{}));
    args.source_key = parameters.value("source_key", std::string{});
    args.binding_source_key = parameters.value("binding_source_key", std::string{});
    args.store_key = parameters.value("store_key", std::string{});
    args.name_prefix = parameters.value("name_prefix", std::string{});
    args.pre = parameters.value("pre", int64_t{0});
    args.post = parameters.value("post", int64_t{0});
    args.window_start = parameters.value("window_start", 0.0);
    args.window_end = parameters.value("window_end", 0.0);

    if (parameters.contains("source_keys")) {
        if (!parameters["source_keys"].is_array()) {
            return std::nullopt;
        }
        for (auto const & source_key: parameters["source_keys"]) {
            if (!source_key.is_string()) {
                return std::nullopt;
            }
            args.source_keys.push_back(source_key.get<std::string>());
        }
    }

    return args;
}

// --- RowModifierRegistry implementation ---

bool RowModifierRegistry::registerModifier(RowModifierDescriptor descriptor) {
    auto const duplicate = std::ranges::any_of(
            _descriptors,
            [&descriptor](RowModifierDescriptor const & existing) {
                return existing.id == descriptor.id;
            });
    if (duplicate) {
        return false;
    }
    _descriptors.push_back(std::move(descriptor));
    return true;
}

RowModifierDescriptor const * RowModifierRegistry::find(std::string const & id) const {
    auto const iter = std::ranges::find_if(
            _descriptors,
            [&id](RowModifierDescriptor const & descriptor) {
                return descriptor.id == id;
            });
    if (iter == _descriptors.end()) {
        return nullptr;
    }
    return &(*iter);
}

std::vector<RowModifierDescriptor const *> RowModifierRegistry::descriptors() const {
    std::vector<RowModifierDescriptor const *> result;
    result.reserve(_descriptors.size());
    for (auto const & descriptor: _descriptors) {
        result.push_back(&descriptor);
    }
    return result;
}

std::vector<RowModifierDescriptor const *> RowModifierRegistry::getModifiersFor(EffectiveRowType row_type) const {
    std::vector<RowModifierDescriptor const *> results;
    for (auto const & desc: _descriptors) {
        if (std::ranges::find(desc.supported_row_types, row_type) != desc.supported_row_types.end()) {
            results.push_back(&desc);
        }
    }
    return results;
}

RowModifierRegistry createBuiltInRowModifierRegistry() {
    RowModifierRegistry registry;
    static_cast<void>(registry.registerModifier(intervalStartModifierDescriptor()));
    static_cast<void>(registry.registerModifier(windowAroundIntervalStartModifierDescriptor()));
    static_cast<void>(registry.registerModifier(bindIntervalStartModifierDescriptor()));
    return registry;
}

// --- ColumnAggregatorRegistry implementation ---

bool ColumnAggregatorRegistry::registerAggregator(ColumnAggregatorDescriptor descriptor) {
    auto const duplicate = std::ranges::any_of(
            _descriptors,
            [&descriptor](ColumnAggregatorDescriptor const & existing) {
                return existing.id == descriptor.id;
            });
    if (duplicate) {
        return false;
    }
    _descriptors.push_back(std::move(descriptor));
    return true;
}

ColumnAggregatorDescriptor const * ColumnAggregatorRegistry::find(std::string const & id) const {
    auto const it = std::ranges::find_if(_descriptors, [&id](ColumnAggregatorDescriptor const & desc) {
        return desc.id == id;
    });
    if (it != _descriptors.end()) {
        return &(*it);
    }
    return nullptr;
}

std::vector<ColumnAggregatorDescriptor const *> ColumnAggregatorRegistry::descriptors() const {
    std::vector<ColumnAggregatorDescriptor const *> results;
    results.reserve(_descriptors.size());
    for (auto const & desc: _descriptors) {
        results.push_back(&desc);
    }
    return results;
}

std::vector<ColumnAggregatorDescriptor const *> ColumnAggregatorRegistry::getAggregatorsFor(EffectiveRowType row_type) const {
    std::vector<ColumnAggregatorDescriptor const *> results;
    for (auto const & desc : _descriptors) {
        if (std::ranges::find(desc.supported_row_types, row_type) != desc.supported_row_types.end()) {
            results.push_back(&desc);
        }
    }
    return results;
}

bool ColumnAggregatorRegistry::isCompatibleComposition(
        RowModifierDescriptor const * modifier,
        ColumnAggregatorDescriptor const & aggregator) {
    auto const & rules = aggregator.composition_rules;

    if (rules.requires_pipeline_value_bindings) {
        if (modifier == nullptr || modifier->id != "bind_interval_start") {
            return false;
        }
    }

    if (rules.required_row_geometry == EffectiveRowType::Interval && modifier != nullptr &&
        modifier->output_row_type == EffectiveRowType::Timestamp) {
        return false;
    }

    if (rules.required_row_geometry == EffectiveRowType::Timestamp) {
        if (modifier == nullptr || modifier->output_row_type != EffectiveRowType::Timestamp) {
            return false;
        }
    }

    if (!rules.compatible_modifier_ids.empty()) {
        auto const allows_no_modifier =
                std::ranges::find(rules.compatible_modifier_ids, std::string{}) !=
                rules.compatible_modifier_ids.end();
        if (modifier == nullptr) {
            if (!allows_no_modifier) {
                return false;
            }
        } else if (!modifierIdInList(modifier, rules.compatible_modifier_ids)) {
            return false;
        }
    }

    if (modifier != nullptr && modifier->output_row_type == EffectiveRowType::Timestamp) {
        auto const supports_timestamp = std::ranges::find(
                                                aggregator.supported_row_types,
                                                EffectiveRowType::Timestamp) !=
                                        aggregator.supported_row_types.end();
        if (!supports_timestamp) {
            return false;
        }
    }

    return true;
}

std::vector<ColumnAggregatorDescriptor const *> ColumnAggregatorRegistry::getAggregatorsFor(
        AggregatorQueryContext const & ctx) const {
    std::vector<ColumnAggregatorDescriptor const *> results;
    for (auto const & desc: _descriptors) {
        if (ctx.tensor_column_only &&
            desc.composition_rules.output_kind != ColumnOutputKind::ScalarFloat) {
            continue;
        }
        if (std::ranges::find(desc.supported_row_types, ctx.effective_row_type) ==
            desc.supported_row_types.end()) {
            continue;
        }
        if (!isCompatibleComposition(ctx.selected_modifier, desc)) {
            continue;
        }
        results.push_back(&desc);
    }
    return results;
}

ColumnAggregatorRegistry createBuiltInColumnAggregatorRegistry() {
    ColumnAggregatorRegistry registry;
    static_cast<void>(registry.registerAggregator(meanValueAggregatorDescriptor()));
    static_cast<void>(registry.registerAggregator(eventCountAggregatorDescriptor()));
    static_cast<void>(registry.registerAggregator(eventPresenceAggregatorDescriptor()));
    static_cast<void>(registry.registerAggregator(analogSampleAggregatorDescriptor()));
    static_cast<void>(registry.registerAggregator(pointXyAggregatorDescriptor()));
    static_cast<void>(registry.registerAggregator(multiPointXyAggregatorDescriptor()));
    static_cast<void>(registry.registerAggregator(rasterEventsRelativeAggregatorDescriptor()));
    static_cast<void>(registry.registerAggregator(trialRelativeEventCountAggregatorDescriptor()));
    return registry;
}

}// namespace Neuralyzer::TensorDesign
