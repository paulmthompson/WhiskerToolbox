#ifndef NEURALYZER_V2_MULTI_INPUT_STEP_CONTEXT_HPP
#define NEURALYZER_V2_MULTI_INPUT_STEP_CONTEXT_HPP

/**
 * @file MultiInputStepContext.hpp
 * @brief UI context for configuring the second input of a binary transform step.
 */

#include <optional>
#include <string>
#include <vector>

/**
 * @brief Context for multi-input step configuration in StepConfigPanel.
 */
struct MultiInputStepContext {
    bool enabled = false;
    std::string primary_input_key;
    std::string primary_input_type_name;
    std::string secondary_input_type_name;
    std::optional<std::string> additional_input_key;
    std::vector<std::string> available_secondary_keys;
};

#endif// NEURALYZER_V2_MULTI_INPUT_STEP_CONTEXT_HPP
