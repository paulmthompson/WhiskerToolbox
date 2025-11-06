#ifndef VARIANT_TYPE_CHECK_HPP
#define VARIANT_TYPE_CHECK_HPP

#include "DataManagerTypes.hpp"
#include <memory>
#include <variant>

/**
 * @brief Template function to check if a DataTypeVariant contains a valid shared_ptr of a specific type.
 *
 * This utility function checks:
 * 1. If the variant holds the correct alternative type (shared_ptr<T>)
 * 2. If the shared_ptr it holds is actually non-null
 *
 * @tparam T The data type to check for (e.g., MaskData, LineData, PointData)
 * @param dataVariant The DataTypeVariant to check
 * @return true if the variant contains a non-null shared_ptr<T>, false otherwise
 *
 * @example
 * DataTypeVariant variant = std::make_shared<MaskData>();
 * bool isValid = canApplyToType<MaskData>(variant); // returns true
 *
 * @example
 * DataTypeVariant variant = std::make_shared<LineData>();
 * bool isValid = canApplyToType<MaskData>(variant); // returns false (wrong type)
 */
template<typename T>
[[nodiscard]] bool canApplyToType(DataTypeVariant const & dataVariant) {
    // 1. Check if the variant holds the correct alternative type (shared_ptr<T>)
    if (!std::holds_alternative<std::shared_ptr<T>>(dataVariant)) {
        return false;
    }

    // 2. Check if the shared_ptr it holds is actually non-null.
    //    Use get_if for safe access (returns nullptr if type is wrong, though step 1 checked)
    auto const * ptr_ptr = std::get_if<std::shared_ptr<T>>(&dataVariant);

    // Return true only if get_if succeeded AND the contained shared_ptr is not null.
    return ptr_ptr && *ptr_ptr;
}

#endif // VARIANT_TYPE_CHECK_HPP
