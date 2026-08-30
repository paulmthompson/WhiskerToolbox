#ifndef NEURALYZER_V2_MULTI_INPUT_KEY_RESOLVER_HPP
#define NEURALYZER_V2_MULTI_INPUT_KEY_RESOLVER_HPP

/**
 * @file MultiInputKeyResolver.hpp
 * @brief Helpers for resolving ordered DataManager keys for binary transforms.
 */

#include <optional>
#include <span>
#include <string>
#include <typeindex>
#include <vector>

namespace Neuralyzer::Transforms::V2 {

/**
 * @brief Multi-input metadata for a registered transform.
 */
struct MultiInputTransformInfo {
    bool is_multi_input = false;
    std::vector<std::type_index> individual_input_types;
};

/**
 * @brief Look up multi-input metadata for a transform by name.
 * @param transform_name Registered transform name
 * @return Metadata when the transform is binary; empty otherwise
 */
[[nodiscard]] std::optional<MultiInputTransformInfo>
getMultiInputTransformInfo(std::string const & transform_name);

/**
 * @brief Map an individual input type (element or container) to a container type_index.
 * @param individual_type Element or container type from registry metadata
 * @return Container type_index suitable for DataManager key filtering
 */
[[nodiscard]] std::type_index individualInputToContainerType(std::type_index individual_type);

/**
 * @brief Determine the secondary container type for a binary transform.
 * @param primary_container Container type of the focused primary input
 * @param individual_input_types Registered input types for the transform
 * @return Secondary container type, or empty if primary does not match either slot
 */
[[nodiscard]] std::optional<std::type_index> getSecondaryContainerType(
        std::type_index primary_container,
        std::span<std::type_index const> individual_input_types);

/**
 * @brief Order primary and secondary keys to match registry input slot order.
 * @param primary_key Focused DataManager key
 * @param primary_container Container type of the focused input
 * @param secondary_key User-selected second DataManager key
 * @param individual_input_types Registered input types for the transform
 * @return (input_key, additional_input_keys[0]) in registry order
 */
[[nodiscard]] std::optional<std::pair<std::string, std::string>> resolveOrderedBinaryInputKeys(
        std::string const & primary_key,
        std::type_index primary_container,
        std::string const & secondary_key,
        std::span<std::type_index const> individual_input_types);

}// namespace Neuralyzer::Transforms::V2

#endif// NEURALYZER_V2_MULTI_INPUT_KEY_RESOLVER_HPP
