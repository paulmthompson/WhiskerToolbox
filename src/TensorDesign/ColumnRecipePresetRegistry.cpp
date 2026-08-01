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

[[nodiscard]] std::optional<ColumnRecipePresetExpansion> expandMeanOverInterval(
        ColumnRecipePresetArgs const & args) {
    if (args.output_name.empty() || args.source_key.empty()) {
        return std::nullopt;
    }

    TensorBuilders::ColumnRecipe recipe;
    recipe.column_name = args.output_name;
    recipe.source_key = args.source_key;
    recipe.pipeline_json = R"({"steps": [], "range_reduction": {"reduction_name": "MeanValue"}})";

    ColumnRecipePresetExpansion expansion;
    expansion.columns.push_back(std::move(recipe));
    return expansion;
}

[[nodiscard]] std::optional<ColumnRecipePresetExpansion> expandEventCountOverInterval(
        ColumnRecipePresetArgs const & args) {
    if (args.output_name.empty() || args.source_key.empty()) {
        return std::nullopt;
    }

    TensorBuilders::ColumnRecipe recipe;
    recipe.column_name = args.output_name;
    recipe.source_key = args.source_key;
    recipe.pipeline_json = R"({"steps": [], "range_reduction": {"reduction_name": "EventCount"}})";

    ColumnRecipePresetExpansion expansion;
    expansion.columns.push_back(std::move(recipe));
    return expansion;
}

[[nodiscard]] std::optional<ColumnRecipePresetExpansion> expandAnalogSampleAtIntervalStart(
        ColumnRecipePresetArgs const & args) {
    if (args.output_name.empty() || args.source_key.empty()) {
        return std::nullopt;
    }

    TensorBuilders::ColumnRecipe recipe;
    recipe.column_name = args.output_name;
    recipe.source_key = args.source_key;
    recipe.row_pipeline_json =
            R"({"steps": [{"step_id": "interval_start", "transform_name": "IntervalToEvent", "parameters": {"point": "start"}}]})";
    recipe.pipeline_json = R"({"steps": []})";

    ColumnRecipePresetExpansion expansion;
    expansion.columns.push_back(std::move(recipe));
    return expansion;
}

[[nodiscard]] std::optional<ColumnRecipePresetExpansion> expandEventPresenceAroundIntervalStart(
        ColumnRecipePresetArgs const & args) {
    if (args.output_name.empty() || args.source_key.empty() || args.pre < 0 || args.post < 0) {
        return std::nullopt;
    }

    TensorBuilders::ColumnRecipe recipe;
    recipe.column_name = args.output_name;
    recipe.source_key = args.source_key;
    recipe.row_pipeline_json =
            R"({"steps": [{"step_id": "interval_start", "transform_name": "IntervalToEvent", "parameters": {"point": "start"}}, {"step_id": "window", "transform_name": "EventToInterval", "parameters": {"pre_expansion": )" +
            std::to_string(args.pre) + R"(, "post_expansion": )" + std::to_string(args.post) + R"(}}]})";
    recipe.pipeline_json = R"({"steps": [], "range_reduction": {"reduction_name": "EventPresence"}})";

    ColumnRecipePresetExpansion expansion;
    expansion.columns.push_back(std::move(recipe));
    return expansion;
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
    recipe.pipeline_json = R"({"steps": [{"step_id": ")" + std::string(step_id) +
                           R"(", "transform_name": "PointCoordinate", "parameters": {"coordinate": ")" +
                           coordinate + R"("}}])";
    if (reduce_mean) {
        recipe.pipeline_json += R"(, "range_reduction": {"reduction_name": "MeanValue"})";
    }
    recipe.pipeline_json += "}";
    return recipe;
}

[[nodiscard]] std::optional<ColumnRecipePresetExpansion> expandPointXy(
        ColumnRecipePresetArgs const & args) {
    if (args.source_key.empty() || args.name_prefix.empty()) {
        return std::nullopt;
    }

    ColumnRecipePresetExpansion expansion;
    expansion.columns.push_back(pointCoordinateRecipe(args.name_prefix + "_x", args.source_key, "x", "X"));
    expansion.columns.push_back(pointCoordinateRecipe(args.name_prefix + "_y", args.source_key, "y", "Y"));
    return expansion;
}

[[nodiscard]] std::optional<ColumnRecipePresetExpansion> expandMultiPointXy(
        ColumnRecipePresetArgs const & args) {
    if (args.source_keys.empty()) {
        return std::nullopt;
    }

    ColumnRecipePresetExpansion expansion;
    expansion.columns.reserve(args.source_keys.size() * 2);
    for (auto const & source_key: args.source_keys) {
        if (source_key.empty()) {
            return std::nullopt;
        }
        expansion.columns.push_back(pointCoordinateRecipe(source_key + "_x", source_key, "x", "X"));
        expansion.columns.push_back(pointCoordinateRecipe(source_key + "_y", source_key, "y", "Y"));
    }
    return expansion;
}

[[nodiscard]] std::optional<ColumnRecipePresetExpansion> expandPointXyAtIntervalStart(
        ColumnRecipePresetArgs const & args) {
    if (args.source_key.empty() || args.name_prefix.empty()) {
        return std::nullopt;
    }

    auto const row_pipeline_json =
            R"({"steps": [{"step_id": "interval_start", "transform_name": "IntervalToEvent", "parameters": {"point": "start"}}]})";

    ColumnRecipePresetExpansion expansion;
    expansion.columns.push_back(pointCoordinateRecipe(
            args.name_prefix + "_x_at_interval_start", args.source_key, "x", "X", row_pipeline_json));
    expansion.columns.push_back(pointCoordinateRecipe(
            args.name_prefix + "_y_at_interval_start", args.source_key, "y", "Y", row_pipeline_json));
    return expansion;
}

[[nodiscard]] ColumnRecipePresetDescriptor meanOverIntervalDescriptor() {
    return ColumnRecipePresetDescriptor{
            .id = "mean_over_interval",
            .display_name = "Mean over interval",
            .description = "Reduce an analog-like source with MeanValue over each row interval.",
            .parameters = meanOverIntervalSchema(),
            .source = ColumnRecipePresetSource::BuiltIn,
            .expand = expandMeanOverInterval};
}

[[nodiscard]] ColumnRecipePresetDescriptor eventCountOverIntervalDescriptor() {
    return ColumnRecipePresetDescriptor{
            .id = "event_count_over_interval",
            .display_name = "Event count over interval",
            .description = "Count events from a source over each row interval.",
            .parameters = outputSourceSchema("EventCountOverIntervalPresetArgs"),
            .source = ColumnRecipePresetSource::BuiltIn,
            .expand = expandEventCountOverInterval};
}

[[nodiscard]] ColumnRecipePresetDescriptor analogSampleAtIntervalStartDescriptor() {
    return ColumnRecipePresetDescriptor{
            .id = "analog_sample_at_interval_start",
            .display_name = "Analog sample at interval start",
            .description = "Sample an analog-like source at each row interval start.",
            .parameters = outputSourceSchema("AnalogSampleAtIntervalStartPresetArgs"),
            .source = ColumnRecipePresetSource::BuiltIn,
            .expand = expandAnalogSampleAtIntervalStart};
}

[[nodiscard]] ColumnRecipePresetDescriptor eventPresenceAroundIntervalStartDescriptor() {
    return ColumnRecipePresetDescriptor{
            .id = "event_presence_around_interval_start",
            .display_name = "Event presence around interval start",
            .description = "Report whether any source event occurs in a window around each row interval start.",
            .parameters = eventWindowSchema(),
            .source = ColumnRecipePresetSource::BuiltIn,
            .expand = expandEventPresenceAroundIntervalStart};
}

[[nodiscard]] ColumnRecipePresetDescriptor pointXyDescriptor() {
    return ColumnRecipePresetDescriptor{
            .id = "point_xy",
            .display_name = "Point x/y",
            .description = "Expand one PointData source into x and y scalar columns.",
            .parameters = pointXySchema(),
            .source = ColumnRecipePresetSource::BuiltIn,
            .expand = expandPointXy};
}

[[nodiscard]] ColumnRecipePresetDescriptor multiPointXyDescriptor() {
    return ColumnRecipePresetDescriptor{
            .id = "multi_point_xy",
            .display_name = "Multiple point x/y",
            .description = "Expand multiple PointData sources into x and y scalar columns.",
            .parameters = multiPointXySchema(),
            .source = ColumnRecipePresetSource::BuiltIn,
            .expand = expandMultiPointXy};
}

[[nodiscard]] ColumnRecipePresetDescriptor pointXyAtIntervalStartDescriptor() {
    return ColumnRecipePresetDescriptor{
            .id = "point_xy_at_interval_start",
            .display_name = "Point x/y at interval start",
            .description = "Sample one PointData source at each row interval start and expand x/y columns.",
            .parameters = pointXySchema(),
            .source = ColumnRecipePresetSource::BuiltIn,
            .expand = expandPointXyAtIntervalStart};
}

}// namespace

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

ColumnRecipePresetRegistry createBuiltInColumnRecipePresetRegistry() {
    ColumnRecipePresetRegistry registry;
    static_cast<void>(registry.registerPreset(meanOverIntervalDescriptor()));
    static_cast<void>(registry.registerPreset(eventCountOverIntervalDescriptor()));
    static_cast<void>(registry.registerPreset(analogSampleAtIntervalStartDescriptor()));
    static_cast<void>(registry.registerPreset(eventPresenceAroundIntervalStartDescriptor()));
    static_cast<void>(registry.registerPreset(pointXyDescriptor()));
    static_cast<void>(registry.registerPreset(multiPointXyDescriptor()));
    static_cast<void>(registry.registerPreset(pointXyAtIntervalStartDescriptor()));
    return registry;
}

std::optional<ColumnRecipePresetArgs> parseColumnRecipePresetArgs(nlohmann::json const & parameters) {
    if (!parameters.is_object()) {
        return std::nullopt;
    }

    ColumnRecipePresetArgs args;
    args.output_name = parameters.value("output_name", parameters.value("name", std::string{}));
    args.source_key = parameters.value("source_key", std::string{});
    args.name_prefix = parameters.value("name_prefix", std::string{});
    args.pre = parameters.value("pre", int64_t{0});
    args.post = parameters.value("post", int64_t{0});

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

}// namespace Neuralyzer::TensorDesign
