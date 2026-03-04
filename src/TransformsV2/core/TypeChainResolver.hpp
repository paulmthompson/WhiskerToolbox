/**
 * @file TypeChainResolver.hpp
 * @brief Static type-chain resolution for transform pipelines
 *
 * Provides a data-free way to walk a sequence of transform step names
 * and resolve the input/output container types at each step.  This
 * mirrors the logic in TransformPipeline::execute() (segment compilation
 * + is_ragged tracking) but without requiring actual data, making it
 * suitable for UI validation and display.
 */

#ifndef WHISKERTOOLBOX_TYPE_CHAIN_RESOLVER_HPP
#define WHISKERTOOLBOX_TYPE_CHAIN_RESOLVER_HPP

#include <span>         // std::span
#include <string>       // std::string
#include <typeindex>    // std::type_index
#include <vector>       // std::vector

namespace WhiskerToolbox::Transforms::V2 {

/**
 * @brief Per-step type resolution result
 */
struct StepTypeInfo {
    std::string input_type_name;   ///< Human-readable input container type
    std::string output_type_name;  ///< Human-readable output container type
    bool is_valid = true;          ///< Whether the step's input type matches the chain
};

/**
 * @brief Result of resolving a type chain through a sequence of transforms
 *
 * Contains per-step info (human-readable type names + validity) as well as
 * the final output types of the chain.
 */
struct TypeChainResult {
    std::vector<StepTypeInfo> steps;
    std::type_index output_element_type{typeid(void)};
    std::type_index output_container_type{typeid(void)};
    bool all_valid = true;
};

/**
 * @brief Resolve the type chain for a sequence of transform names
 *
 * Starting from @p input_container_type, walks the transforms in order
 * using metadata from the ElementRegistry.  For each step the function
 * records:
 *  - the human-readable input container name
 *  - the human-readable output container name
 *  - whether the step's required input matches the running type
 *
 * The ragged-container tracking mirrors TransformPipeline::execute():
 *  - time-grouped + produces_single_output → ragged becomes non-ragged
 *  - time-grouped + !produces_single_output → becomes ragged
 *  - element-wise transforms preserve the ragged state
 *
 * @param input_container_type The container type_index feeding the chain
 *        (e.g. typeid(MaskData), typeid(AnalogTimeSeries))
 * @param step_names Ordered transform names (element *or* container level)
 * @return TypeChainResult with per-step info and final output types
 */
TypeChainResult resolveTypeChain(
        std::type_index input_container_type,
        std::span<std::string const> step_names);

} // namespace WhiskerToolbox::Transforms::V2

#endif // WHISKERTOOLBOX_TYPE_CHAIN_RESOLVER_HPP
