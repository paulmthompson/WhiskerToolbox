#include "JsonColumnAggregator.hpp"

namespace Neuralyzer::TensorDesign {

namespace {

EffectiveRowType parseEffectiveRowType(std::string const& str) {
    if (str == "Timestamp") return EffectiveRowType::Timestamp;
    if (str == "Interval") return EffectiveRowType::Interval;
    return EffectiveRowType::Unchanged;
}

ColumnOutputKind parseColumnOutputKind(std::string const& str) {
    if (str == "GatheredDataObject") return ColumnOutputKind::GatheredDataObject;
    return ColumnOutputKind::ScalarFloat;
}

GatheredOutputType parseGatheredOutputType(std::string const& str) {
    if (str == "DigitalEventSeries") return GatheredOutputType::DigitalEventSeries;
    return GatheredOutputType::Unspecified;
}

} // namespace

JsonColumnAggregator::JsonColumnAggregator(JsonColumnAggregatorPresetDto data) : _data(std::move(data)) {
    for (auto const& type_str : _data.supported_row_types) {
        _supported_row_types.push_back(parseEffectiveRowType(type_str));
    }
    _composition_rules.output_kind = parseColumnOutputKind(_data.composition_rules.output_kind);
    _composition_rules.gathered_output_type = parseGatheredOutputType(_data.composition_rules.gathered_output_type);
    _composition_rules.requires_pipeline_value_bindings = _data.composition_rules.requires_pipeline_value_bindings;
    _composition_rules.required_row_geometry = parseEffectiveRowType(_data.composition_rules.required_row_geometry);
    _composition_rules.compatible_modifier_ids = _data.composition_rules.compatible_modifier_ids;
}

std::string JsonColumnAggregator::id() const { return _data.id; }
std::string JsonColumnAggregator::display_name() const { return _data.display_name; }
std::string JsonColumnAggregator::description() const { return _data.description; }
std::vector<EffectiveRowType> JsonColumnAggregator::supported_row_types() const { return _supported_row_types; }
AggregatorCompositionRules JsonColumnAggregator::composition_rules() const { return _composition_rules; }
ParameterSchema JsonColumnAggregator::parameters() const { return _data.parameters; }

std::optional<ColumnAggregatorExpansion> JsonColumnAggregator::expand(ColumnRecipePresetArgs const & args) const {
    if (args.output_name.empty() || args.source_key.empty()) return std::nullopt;
    
    TensorBuilders::ColumnRecipe recipe;
    recipe.column_name = args.output_name;
    recipe.source_key = args.source_key;
    
    // Very basic templating for now: replace known markers.
    // Real implementation would parse the JSON and inject bindings, or use a true template engine.
    std::string pipeline_json = _data.pipeline_json_template;
    
    // As a placeholder for Phase 9g, we simply use the template directly.
    recipe.pipeline_json = pipeline_json;
    
    return ColumnAggregatorExpansion{.columns = {std::move(recipe)}};
}

} // namespace Neuralyzer::TensorDesign
