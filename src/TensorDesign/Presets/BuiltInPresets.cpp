#include "BuiltInPresets.hpp"
#include "PresetInterfaces.hpp"

#include "DataManager/DataManager.hpp"
#include "Points/Point_Data.hpp"

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
            .tooltip = "Prefix for generated x and y column names.",
            .is_optional = true});
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
            .tooltip = "PointData keys to expand into x and y columns.",
            .is_vector = true,
            .vector_element_type = "std::string",
            .dynamic_combo = true});
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
    std::string const reduction = reduce_mean ? "Mean" : "First";
    recipe.pipeline_json = R"({"steps": [{"step_id": ")" + std::string(step_id) +
                           R"(", "transform_name": "PointCoordinateReduction", "parameters": {"coordinate": ")" +
                           coordinate + R"(", "reduction": ")" + reduction + R"("}}]})";
    return recipe;
}

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

[[nodiscard]] std::string normalizeEventsRelativePipelineJson(std::string const & store_key) {
    return R"({"steps": [{"step_id": "normalize", "transform_name": "NormalizeDigitalEventSeriesRelative", "parameters": {"alignment_time": 0}, "param_bindings": {"alignment_time": ")" +
           store_key + R"("}}]})";
}

}// namespace

// --- IntervalStartModifier ---
std::string IntervalStartModifier::id() const { return "interval_start"; }
std::string IntervalStartModifier::display_name() const { return "At interval start"; }
std::string IntervalStartModifier::description() const { return "Convert an interval to an event at its start."; }
EffectiveRowType IntervalStartModifier::output_row_type() const { return EffectiveRowType::Timestamp; }
std::vector<EffectiveRowType> IntervalStartModifier::supported_row_types() const { return {EffectiveRowType::Interval}; }
ParameterSchema IntervalStartModifier::parameters() const { return ParameterSchema{.params_type_name = "IntervalStartArgs"}; }
std::optional<RowModifierExpansion> IntervalStartModifier::expand(ColumnRecipePresetArgs const &) const {
    return RowModifierExpansion{.row_pipeline_json = intervalStartPipelineJson()};
}

// --- WindowAroundIntervalStartModifier ---
std::string WindowAroundIntervalStartModifier::id() const { return "window_around_interval_start"; }
std::string WindowAroundIntervalStartModifier::display_name() const { return "Window around interval start"; }
std::string WindowAroundIntervalStartModifier::description() const { return "Create a window around the interval start."; }
EffectiveRowType WindowAroundIntervalStartModifier::output_row_type() const { return EffectiveRowType::Interval; }
std::vector<EffectiveRowType> WindowAroundIntervalStartModifier::supported_row_types() const { return {EffectiveRowType::Interval}; }
ParameterSchema WindowAroundIntervalStartModifier::parameters() const { return rowModifierWindowSchema(); }
std::optional<RowModifierExpansion> WindowAroundIntervalStartModifier::expand(ColumnRecipePresetArgs const & args) const {
    if (args.pre < 0 || args.post < 0) {
        return std::nullopt;
    }
    return RowModifierExpansion{
            .row_pipeline_json = R"({"steps": [{"step_id": "interval_start", "transform_name": "IntervalToEvent", "parameters": {"point": "start"}}, {"step_id": "window", "transform_name": "EventToInterval", "parameters": {"pre_expansion": )" +
                                 std::to_string(args.pre) + R"(, "post_expansion": )" + std::to_string(args.post) + R"(}}]})"};
}

// --- BindIntervalStartModifier ---
std::string BindIntervalStartModifier::id() const { return "bind_interval_start"; }
std::string BindIntervalStartModifier::display_name() const { return "Bind interval start"; }
std::string BindIntervalStartModifier::description() const { return "Bind the interval start time to a variable for use in the column pipeline, without modifying the row shape."; }
EffectiveRowType BindIntervalStartModifier::output_row_type() const { return EffectiveRowType::Unchanged; }
std::vector<EffectiveRowType> BindIntervalStartModifier::supported_row_types() const { return {EffectiveRowType::Interval}; }
ParameterSchema BindIntervalStartModifier::parameters() const {
    return ParameterSchema{.params_type_name = "BindIntervalStartArgs",
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
                                           .tooltip = "PipelineValueStore key used for row alignment time."}}};
}
std::optional<RowModifierExpansion> BindIntervalStartModifier::expand(ColumnRecipePresetArgs const & args) const {
    RowModifierExpansion exp;
    exp.pipeline_value_bindings.push_back(intervalStartBinding(args));
    return exp;
}

// --- MeanValueAggregator ---
std::string MeanValueAggregator::id() const { return "mean_value"; }
std::string MeanValueAggregator::display_name() const { return "Mean Value"; }
std::string MeanValueAggregator::description() const { return "Calculate the mean of an analog signal over the row interval."; }
std::vector<EffectiveRowType> MeanValueAggregator::supported_row_types() const { return {EffectiveRowType::Interval}; }
AggregatorCompositionRules MeanValueAggregator::composition_rules() const {
    return {
            .output_kind = ColumnOutputKind::ScalarFloat,
            .required_row_geometry = EffectiveRowType::Interval,
            .compatible_modifier_ids = {"", "window_around_interval_start"},
            .supported_data_types = {DM_DataType::Analog, DM_DataType::RaggedAnalog}};
}
ParameterSchema MeanValueAggregator::parameters() const { return outputSourceSchema("MeanValueArgs"); }
std::optional<ColumnAggregatorExpansion> MeanValueAggregator::expand(ColumnRecipePresetArgs const & args) const {
    if (args.output_name.empty() || args.source_key.empty()) return std::nullopt;
    TensorBuilders::ColumnRecipe recipe;
    recipe.column_name = args.output_name;
    recipe.source_key = args.source_key;
    recipe.pipeline_json = R"({"steps": [], "range_reduction": {"reduction_name": "MeanValue"}})";
    return ColumnAggregatorExpansion{.columns = {std::move(recipe)}};
}

// --- EventCountAggregator ---
std::string EventCountAggregator::id() const { return "event_count"; }
std::string EventCountAggregator::display_name() const { return "Event Count"; }
std::string EventCountAggregator::description() const { return "Count the total number of events within the row interval."; }
std::vector<EffectiveRowType> EventCountAggregator::supported_row_types() const { return {EffectiveRowType::Interval}; }
AggregatorCompositionRules EventCountAggregator::composition_rules() const {
    return {
            .output_kind = ColumnOutputKind::ScalarFloat,
            .required_row_geometry = EffectiveRowType::Interval,
            .compatible_modifier_ids = {"", "window_around_interval_start"},
            .supported_data_types = {DM_DataType::DigitalEvent, DM_DataType::DigitalInterval}};
}
ParameterSchema EventCountAggregator::parameters() const { return outputSourceSchema("EventCountArgs"); }
std::optional<ColumnAggregatorExpansion> EventCountAggregator::expand(ColumnRecipePresetArgs const & args) const {
    if (args.output_name.empty() || args.source_key.empty()) return std::nullopt;
    TensorBuilders::ColumnRecipe recipe;
    recipe.column_name = args.output_name;
    recipe.source_key = args.source_key;
    recipe.pipeline_json = R"({"steps": [], "range_reduction": {"reduction_name": "EventCount"}})";
    return ColumnAggregatorExpansion{.columns = {std::move(recipe)}};
}

// --- EventPresenceAggregator ---
std::string EventPresenceAggregator::id() const { return "event_presence"; }
std::string EventPresenceAggregator::display_name() const { return "Event Presence"; }
std::string EventPresenceAggregator::description() const { return "Report whether any event is present within the row interval."; }
std::vector<EffectiveRowType> EventPresenceAggregator::supported_row_types() const { return {EffectiveRowType::Interval}; }
AggregatorCompositionRules EventPresenceAggregator::composition_rules() const {
    return {
            .output_kind = ColumnOutputKind::ScalarFloat,
            .required_row_geometry = EffectiveRowType::Interval,
            .compatible_modifier_ids = {"", "window_around_interval_start"},
            .supported_data_types = {DM_DataType::DigitalEvent, DM_DataType::DigitalInterval}};
}
ParameterSchema EventPresenceAggregator::parameters() const { return outputSourceSchema("EventPresenceArgs"); }
std::optional<ColumnAggregatorExpansion> EventPresenceAggregator::expand(ColumnRecipePresetArgs const & args) const {
    if (args.output_name.empty() || args.source_key.empty()) return std::nullopt;
    TensorBuilders::ColumnRecipe recipe;
    recipe.column_name = args.output_name;
    recipe.source_key = args.source_key;
    recipe.pipeline_json = R"({"steps": [], "range_reduction": {"reduction_name": "EventPresence"}})";
    return ColumnAggregatorExpansion{.columns = {std::move(recipe)}};
}

// --- AnalogSampleAggregator ---
std::string AnalogSampleAggregator::id() const { return "analog_sample"; }
std::string AnalogSampleAggregator::display_name() const { return "Analog Sample"; }
std::string AnalogSampleAggregator::description() const { return "Sample an analog signal at a specific timestamp."; }
std::vector<EffectiveRowType> AnalogSampleAggregator::supported_row_types() const { return {EffectiveRowType::Timestamp}; }
AggregatorCompositionRules AnalogSampleAggregator::composition_rules() const {
    return {
            .output_kind = ColumnOutputKind::ScalarFloat,
            .required_row_geometry = EffectiveRowType::Timestamp,
            .compatible_modifier_ids = {"", "interval_start"},
            .supported_data_types = {DM_DataType::Analog, DM_DataType::RaggedAnalog}};
}
ParameterSchema AnalogSampleAggregator::parameters() const { return outputSourceSchema("AnalogSampleArgs"); }
std::optional<ColumnAggregatorExpansion> AnalogSampleAggregator::expand(ColumnRecipePresetArgs const & args) const {
    if (args.output_name.empty() || args.source_key.empty()) return std::nullopt;
    TensorBuilders::ColumnRecipe recipe;
    recipe.column_name = args.output_name;
    recipe.source_key = args.source_key;
    recipe.pipeline_json = R"({"steps": []})";
    return ColumnAggregatorExpansion{.columns = {std::move(recipe)}};
}

// --- PointXyAggregator ---
std::string PointXyAggregator::id() const { return "point_xy"; }
std::string PointXyAggregator::display_name() const { return "Point XY"; }
std::string PointXyAggregator::description() const { return "Expand a PointData keypoint into x and y columns at the row timestamp."; }
std::vector<EffectiveRowType> PointXyAggregator::supported_row_types() const { return {EffectiveRowType::Timestamp}; }
AggregatorCompositionRules PointXyAggregator::composition_rules() const {
    return {
            .output_kind = ColumnOutputKind::ScalarFloat,
            .required_row_geometry = EffectiveRowType::Timestamp,
            .compatible_modifier_ids = {"", "interval_start"},
            .supported_data_types = {DM_DataType::Points}};
}
ParameterSchema PointXyAggregator::parameters() const { return pointXySchema(); }
std::optional<ColumnAggregatorExpansion> PointXyAggregator::expand(ColumnRecipePresetArgs const & args) const {
    if (args.source_key.empty()) return std::nullopt;
    std::string const prefix = args.name_prefix.empty() ? args.source_key : args.name_prefix;
    return ColumnAggregatorExpansion{.columns = {
                                             pointCoordinateRecipe(prefix + "_x", args.source_key, "x", "X"),
                                             pointCoordinateRecipe(prefix + "_y", args.source_key, "y", "Y")}};
}

// --- MultiPointXyAggregator ---
std::string MultiPointXyAggregator::id() const { return "multi_point_xy"; }
std::string MultiPointXyAggregator::display_name() const { return "Multi-Point XY"; }
std::string MultiPointXyAggregator::description() const { return "Expand multiple PointData keypoints into x and y columns at the row timestamp."; }
std::vector<EffectiveRowType> MultiPointXyAggregator::supported_row_types() const { return {EffectiveRowType::Timestamp}; }
AggregatorCompositionRules MultiPointXyAggregator::composition_rules() const {
    return {
            .output_kind = ColumnOutputKind::ScalarFloat,
            .required_row_geometry = EffectiveRowType::Timestamp,
            .compatible_modifier_ids = {"", "interval_start"},
            .supported_data_types = {DM_DataType::Points}};
}
ParameterSchema MultiPointXyAggregator::parameters() const { return multiPointXySchema(); }
std::optional<ColumnAggregatorExpansion> MultiPointXyAggregator::expand(ColumnRecipePresetArgs const & args) const {
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
std::vector<std::string> MultiPointXyAggregator::getDynamicOptions(std::string const & field_name, DataManager const * data_manager) const {
    if (field_name == "source_keys" && data_manager) {
        return data_manager->getKeys<PointData>();
    }
    return {};
}

// --- RasterEventsRelativeAggregator ---
std::string RasterEventsRelativeAggregator::id() const { return "raster_events_relative"; }
std::string RasterEventsRelativeAggregator::display_name() const { return "Raster Events (Relative)"; }
std::string RasterEventsRelativeAggregator::description() const { return "Gather events over each row interval and normalize to alignment time. Intermediate gather transform; use trial_relative_event_count for a scalar tensor column."; }
std::vector<EffectiveRowType> RasterEventsRelativeAggregator::supported_row_types() const { return {EffectiveRowType::Interval}; }
AggregatorCompositionRules RasterEventsRelativeAggregator::composition_rules() const {
    return {
            .output_kind = ColumnOutputKind::GatheredDataObject,
            .gathered_output_type = GatheredOutputType::DigitalEventSeries,
            .requires_pipeline_value_bindings = true,
            .required_row_geometry = EffectiveRowType::Interval,
            .compatible_modifier_ids = {"bind_interval_start"},
            .supported_data_types = {DM_DataType::DigitalEvent, DM_DataType::DigitalInterval}};
}
ParameterSchema RasterEventsRelativeAggregator::parameters() const { return trialRelativeEventSchema("RasterEventsRelativeAggregatorArgs"); }
std::optional<ColumnAggregatorExpansion> RasterEventsRelativeAggregator::expand(ColumnRecipePresetArgs const & args) const {
    if (args.output_name.empty() || args.source_key.empty() || bindingSourceKey(args).empty()) {
        return std::nullopt;
    }
    TensorBuilders::ColumnRecipe recipe;
    recipe.column_name = args.output_name;
    recipe.source_key = args.source_key;
    recipe.pipeline_json = normalizeEventsRelativePipelineJson(args.store_key);
    return ColumnAggregatorExpansion{.columns = {std::move(recipe)}};
}

// --- TrialRelativeEventCountAggregator ---
std::string TrialRelativeEventCountAggregator::id() const { return "trial_relative_event_count"; }
std::string TrialRelativeEventCountAggregator::display_name() const { return "Trial Relative Event Count"; }
std::string TrialRelativeEventCountAggregator::description() const { return "Count events in a relative window aligned to a bound timestamp variable."; }
std::vector<EffectiveRowType> TrialRelativeEventCountAggregator::supported_row_types() const { return {EffectiveRowType::Interval}; }
AggregatorCompositionRules TrialRelativeEventCountAggregator::composition_rules() const {
    return {
            .output_kind = ColumnOutputKind::ScalarFloat,
            .requires_pipeline_value_bindings = true,
            .required_row_geometry = EffectiveRowType::Interval,
            .compatible_modifier_ids = {"bind_interval_start"},
            .supported_data_types = {DM_DataType::DigitalEvent, DM_DataType::DigitalInterval}};
}
ParameterSchema TrialRelativeEventCountAggregator::parameters() const { return trialRelativeEventSchema("TrialRelativeEventCountAggregatorArgs"); }
std::optional<ColumnAggregatorExpansion> TrialRelativeEventCountAggregator::expand(ColumnRecipePresetArgs const & args) const {
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

}// namespace Neuralyzer::TensorDesign
