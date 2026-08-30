/**
 * @file MultiInputKeyResolver.cpp
 * @brief Implementation of binary transform DataManager key resolution helpers.
 */

#include "MultiInputKeyResolver.hpp"

#include "TransformsV2/core/ElementRegistry.hpp"

#include "DataManager/utils/ContainerTypeIndex.hpp"

namespace Neuralyzer::Transforms::V2 {

using Neuralyzer::TypeTraits::TypeIndexMapper;

namespace {

std::optional<size_t> findMatchingInputSlot(
        std::type_index container_type,
        std::span<std::type_index const> individual_input_types) {
    for (size_t i = 0; i < individual_input_types.size(); ++i) {
        if (individualInputToContainerType(individual_input_types[i]) == container_type) {
            return i;
        }
    }
    return std::nullopt;
}

}// namespace

std::type_index individualInputToContainerType(std::type_index individual_type) {
    try {
        return TypeIndexMapper::elementToContainer(individual_type);
    } catch (...) {
        return individual_type;
    }
}

std::optional<MultiInputTransformInfo> getMultiInputTransformInfo(std::string const & transform_name) {
    auto & registry = ElementRegistry::instance();

    if (auto const * meta = registry.getMetadata(transform_name)) {
        if (!meta->is_multi_input || meta->individual_input_types.size() < 2) {
            return std::nullopt;
        }
        return MultiInputTransformInfo{
                .is_multi_input = true,
                .individual_input_types = meta->individual_input_types};
    }

    if (auto const * cmeta = registry.getContainerMetadata(transform_name)) {
        if (!cmeta->is_multi_input || cmeta->individual_input_types.size() < 2) {
            return std::nullopt;
        }
        return MultiInputTransformInfo{
                .is_multi_input = true,
                .individual_input_types = cmeta->individual_input_types};
    }

    return std::nullopt;
}

std::optional<std::type_index> getSecondaryContainerType(
        std::type_index primary_container,
        std::span<std::type_index const> individual_input_types) {
    if (individual_input_types.size() < 2) {
        return std::nullopt;
    }

    auto const primary_slot = findMatchingInputSlot(primary_container, individual_input_types);
    if (!primary_slot.has_value()) {
        return std::nullopt;
    }

    size_t const secondary_slot = (*primary_slot == 0) ? 1 : 0;
    return individualInputToContainerType(individual_input_types[secondary_slot]);
}

std::optional<std::pair<std::string, std::string>> resolveOrderedBinaryInputKeys(
        std::string const & primary_key,
        std::type_index primary_container,
        std::string const & secondary_key,
        std::span<std::type_index const> individual_input_types) {
    if (primary_key.empty() || secondary_key.empty() || individual_input_types.size() < 2) {
        return std::nullopt;
    }

    if (primary_key == secondary_key) {
        return std::nullopt;
    }

    auto const primary_slot = findMatchingInputSlot(primary_container, individual_input_types);
    if (!primary_slot.has_value()) {
        return std::nullopt;
    }

    if (*primary_slot == 0) {
        return std::pair<std::string, std::string>{primary_key, secondary_key};
    }
    return std::pair<std::string, std::string>{secondary_key, primary_key};
}

}// namespace Neuralyzer::Transforms::V2
