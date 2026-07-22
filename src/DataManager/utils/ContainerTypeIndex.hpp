/// @file ContainerTypeIndex.hpp
/// @brief Runtime std::type_index and string mapping for DataManager container types.

#ifndef NEURALYZER_CONTAINER_TYPE_INDEX_HPP
#define NEURALYZER_CONTAINER_TYPE_INDEX_HPP

#include <string>
#include <typeindex>

namespace Neuralyzer::TypeTraits {

/// @brief Runtime mapping between element/container types, strings, and raggedness.
class TypeIndexMapper {
public:
    /// @brief Map element type_index to default container type_index.
    /// @note float maps to RaggedAnalogTimeSeries; see TypeChainResolver for contextual analog vs ragged.
    [[nodiscard]] static std::type_index elementToContainer(std::type_index element_type);

    /// @brief Map container type_index to element type_index.
    [[nodiscard]] static std::type_index containerToElement(std::type_index container_type);

    /// @brief Long container name (e.g. "MaskData") or "Unknown".
    [[nodiscard]] static std::string containerToString(std::type_index container_type);

    /// @brief Resolve long or short container name to type_index.
    /// @throws std::runtime_error if name is unknown.
    [[nodiscard]] static std::type_index stringToContainer(std::string const & name);

    /// @brief Whether the container holds multiple elements per time frame.
    /// @return false for unknown types.
    [[nodiscard]] static bool isContainerRagged(std::type_index container_type);
};

} // namespace Neuralyzer::TypeTraits

#endif // NEURALYZER_CONTAINER_TYPE_INDEX_HPP
