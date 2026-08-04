#ifndef TENSORDESIGN_JSONPRESETDTO_HPP
#define TENSORDESIGN_JSONPRESETDTO_HPP

#include "ParameterSchema/ParameterSchema.hpp"
#include <string>

namespace Neuralyzer::TensorDesign {

struct JsonRowModifierPresetDto {
    std::string id;
    std::string display_name;
    std::string description;
    std::string output_row_type;
    std::vector<std::string> supported_row_types;
    ParameterSchema parameters;
    std::string row_pipeline_json_template;
    // Note: pipeline_value_bindings could be added later for JSON modifiers
};

struct JsonColumnAggregatorCompositionRulesDto {
    std::string output_kind = "ScalarFloat";
    std::string gathered_output_type = "Unspecified";
    bool requires_pipeline_value_bindings = false;
    std::string required_row_geometry = "Interval";
    std::vector<std::string> compatible_modifier_ids;
};

struct JsonColumnAggregatorPresetDto {
    std::string id;
    std::string display_name;
    std::string description;
    std::vector<std::string> supported_row_types;
    JsonColumnAggregatorCompositionRulesDto composition_rules;
    ParameterSchema parameters;
    std::string pipeline_json_template;
};

} // namespace Neuralyzer::TensorDesign

#endif // TENSORDESIGN_JSONPRESETDTO_HPP
