#ifndef LOADER_OPTIONS_CONCEPTS_HPP
#define LOADER_OPTIONS_CONCEPTS_HPP

#include <concepts>
#include <string>
#include <type_traits>

namespace WhiskerToolbox {

/**
 * @brief Concept to ensure loader options structs use consistent field naming
 * 
 * Loader options that receive a filepath from DataManager should have a 
 * `filepath` member (not `filename`). This prevents accidental inconsistencies
 * with the DataManager JSON config which always uses `filepath`.
 * 
 * Usage:
 * @code
 * template<HasFilepath Options>
 * std::shared_ptr<DataType> load(Options const& opts);
 * 
 * // Or with static_assert in non-template functions:
 * std::shared_ptr<DataType> load(MyLoaderOptions const& opts) {
 *     static_assert(HasFilepath<MyLoaderOptions>, 
 *         "MyLoaderOptions must have a 'filepath' field");
 *     // ...
 * }
 * @endcode
 */
template<typename T>
concept HasFilepath = requires(T opts) {
    { opts.filepath } -> std::convertible_to<std::string>;
};

/**
 * @brief Helper to detect if a type has a `data_type` member
 * 
 * This is used internally by NoReservedDataTypeField concept.
 */
template<typename T, typename = void>
struct has_data_type_member : std::false_type {};

template<typename T>
struct has_data_type_member<T, std::void_t<decltype(std::declval<T>().data_type)>> : std::true_type {};

/**
 * @brief Helper to detect if a type has a `name` member
 * 
 * This is used internally by NoReservedNameField concept.
 */
template<typename T, typename = void>
struct has_name_member : std::false_type {};

template<typename T>
struct has_name_member<T, std::void_t<decltype(std::declval<T>().name)>> : std::true_type {};

/**
 * @brief Concept to prevent accidental reuse of the `data_type` field name
 * 
 * The field `data_type` is reserved for DataManager-level config to specify
 * the type of data (e.g., "analog", "points", "mask"). Loader options structs
 * should NOT have a `data_type` field to avoid confusion.
 * 
 * If you need to specify a data format within a loader (e.g., binary data type),
 * use alternative names like:
 * - `binary_data_type`
 * - `storage_type` 
 * - `format_type`
 */
template<typename T>
concept NoReservedDataTypeField = !has_data_type_member<T>::value;

/**
 * @brief Concept to prevent accidental reuse of the `name` field name
 * 
 * The field `name` is reserved for DataManager-level config to specify
 * the key under which data will be stored. Loader options structs should
 * NOT have a `name` field.
 */
template<typename T>
concept NoReservedNameField = !has_name_member<T>::value;

/**
 * @brief Combined concept for loader options that are loaded via JSON
 * 
 * Ensures that loader options:
 * 1. Have a `filepath` field (not `filename`) for consistency with DataManager JSON
 * 2. Do NOT have a `data_type` field (reserved for DataManager)
 * 3. Do NOT have a `name` field (reserved for DataManager)
 * 
 * Usage:
 * @code
 * static_assert(ValidLoaderOptions<BinaryAnalogLoaderOptions>,
 *     "BinaryAnalogLoaderOptions must conform to loader options requirements");
 * @endcode
 */
template<typename T>
concept ValidLoaderOptions = HasFilepath<T> && NoReservedDataTypeField<T> && NoReservedNameField<T>;

/**
 * @brief Concept for loader options that don't need filepath
 * 
 * Some loader options (like those for internal transforms) may not need
 * a filepath. This concept only checks for reserved field avoidance.
 */
template<typename T>
concept ValidInternalLoaderOptions = NoReservedDataTypeField<T> && NoReservedNameField<T>;

} // namespace WhiskerToolbox

#endif // LOADER_OPTIONS_CONCEPTS_HPP
