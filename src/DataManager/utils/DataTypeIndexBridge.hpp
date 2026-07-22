/// @file DataTypeIndexBridge.hpp
/// @brief Bridge DM_DataType enum values to container std::type_index.

#ifndef NEURALYZER_DATA_TYPE_INDEX_BRIDGE_HPP
#define NEURALYZER_DATA_TYPE_INDEX_BRIDGE_HPP

#include "DataTypeEnum/DM_DataType.hpp"

#include <optional>
#include <typeindex>

namespace Neuralyzer::TypeTraits {

/// @brief Map DM_DataType to the corresponding container std::type_index.
/// @throws std::runtime_error for types that cannot be used as pipeline sources
///         (Video, Images, Tensor, Time, Unknown).
[[nodiscard]] std::type_index dmDataTypeToContainerTypeIndex(DM_DataType type);

/// @brief Reverse map container type_index to DM_DataType when unambiguous.
[[nodiscard]] std::optional<DM_DataType> containerTypeIndexToDmDataType(std::type_index container_type);

} // namespace Neuralyzer::TypeTraits

#endif // NEURALYZER_DATA_TYPE_INDEX_BRIDGE_HPP
