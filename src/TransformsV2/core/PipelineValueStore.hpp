#ifndef WHISKERTOOLBOX_V2_PIPELINE_VALUE_STORE_HPP
#define WHISKERTOOLBOX_V2_PIPELINE_VALUE_STORE_HPP

/**
 * @file PipelineValueStore.hpp
 * @brief Runtime key-value store for pipeline intermediate values
 *
 * The PipelineValueStore provides a generic mechanism for storing named scalar
 * values that can be bound to transform parameters. This enables composable
 * pipelines where reduction outputs can be wired into transform parameters
 * via JSON configuration.
 *
 * ## Design Principles
 *
 * 1. **Type-safe storage** - Values are stored with their original type
 * 2. **JSON interchange** - All values can be serialized as JSON fragments for reflect-cpp binding
 * 3. **Simple flat namespace** - Keys are plain strings without hierarchy
 * 4. **Immutable semantics** - Values are set once and read many times
 *
 * ## Typical Usage Flow
 *
 * ```
 * ┌─────────────────────────────────────────────────────────────────┐
 * │                    PipelineValueStore                           │
 * │  ┌─────────────────────────────────────────────────────────┐   │
 * │  │  "mean" → 0.5f, "std" → 0.1f, "alignment_time" → 100   │   │
 * │  └─────────────────────────────────────────────────────────┘   │
 * │        ↑                              ↓                         │
 * │   Range Reductions               Param Bindings                 │
 * │   (compute scalars)          (inject into params)               │
 * └─────────────────────────────────────────────────────────────────┘
 * ```
 *
 * ## Example
 *
 * ```cpp
 * PipelineValueStore store;
 *
 * // Reductions populate the store
 * store.set("computed_mean", 0.5f);
 * store.set("computed_std", 0.1f);
 * store.set("alignment_time", int64_t{1000});
 *
 * // Later, bindings read from the store
 * auto mean_json = store.getJson("computed_mean");  // "0.5"
 * auto mean_val = store.getFloat("computed_mean");  // 0.5f
 * ```
 *
 * @see ParameterBinding.hpp for applying store values to parameters
 * @see TransformPipeline for integration with pipeline execution
 */

#include <cstdint>
#include <format>
#include <optional>
#include <string>
#include <unordered_map>
#include <variant>
#include <vector>

namespace WhiskerToolbox::Transforms::V2 {

// ============================================================================
// Supported Value Types
// ============================================================================

/**
 * @brief Variant of all types supported by PipelineValueStore
 *
 * The store supports:
 * - float: For floating-point scalars (statistics, measurements)
 * - int64_t: For integer values (indices, counts, timestamps)
 * - std::string: For string values (labels, categories)
 */
using PipelineValue = std::variant<float, int64_t, std::string>;

// ============================================================================
// PipelineValueStore Implementation
// ============================================================================

/**
 * @brief Runtime key-value store for pipeline intermediate values
 *
 * Stores named scalar values (float, int, int64_t, string) that can be
 * bound to transform parameters. Uses JSON string representation as
 * interchange format for maximum flexibility with reflect-cpp.
 */
class PipelineValueStore {
public:
    PipelineValueStore() = default;

    // ========================================================================
    // Type-safe Setters
    // ========================================================================

    /**
     * @brief Store a floating-point value
     * @param key Unique key for the value
     * @param value The float value to store
     */
    void set(std::string const& key, float value) {
        values_[key] = value;
    }

    /**
     * @brief Store an integer value (promoted to int64_t)
     * @param key Unique key for the value
     * @param value The int value to store
     */
    void set(std::string const& key, int value) {
        values_[key] = static_cast<int64_t>(value);
    }

    /**
     * @brief Store a 64-bit integer value
     * @param key Unique key for the value
     * @param value The int64_t value to store
     */
    void set(std::string const& key, int64_t value) {
        values_[key] = value;
    }

    /**
     * @brief Store a string value
     * @param key Unique key for the value
     * @param value The string value to store
     */
    void set(std::string const& key, std::string value) {
        values_[key] = std::move(value);
    }

    // ========================================================================
    // JSON Accessors (for parameter binding)
    // ========================================================================

    /**
     * @brief Get value as JSON fragment string
     *
     * Returns the value serialized as a JSON fragment suitable for
     * injecting into a larger JSON object via reflect-cpp.
     *
     * - float: "0.5"
     * - int64_t: "100"
     * - string: "\"value\"" (quoted)
     *
     * @param key Key to look up
     * @return JSON string representation, or std::nullopt if key not found
     */
    [[nodiscard]] std::optional<std::string> getJson(std::string const& key) const {
        auto it = values_.find(key);
        if (it == values_.end()) {
            return std::nullopt;
        }

        return std::visit([](auto const& val) -> std::string {
            using T = std::decay_t<decltype(val)>;
            if constexpr (std::is_same_v<T, float>) {
                // Use sufficient precision for float
                return std::format("{}", val);
            } else if constexpr (std::is_same_v<T, int64_t>) {
                return std::format("{}", val);
            } else if constexpr (std::is_same_v<T, std::string>) {
                // JSON-encode string with quotes
                return std::format("\"{}\"", val);
            }
        }, it->second);
    }

    // ========================================================================
    // Typed Getters (for direct access)
    // ========================================================================

    /**
     * @brief Get value as float
     *
     * Performs type conversion if necessary:
     * - float: returns directly
     * - int64_t: converts to float
     * - string: returns nullopt
     *
     * @param key Key to look up
     * @return Float value, or std::nullopt if key not found or incompatible type
     */
    [[nodiscard]] std::optional<float> getFloat(std::string const& key) const {
        auto it = values_.find(key);
        if (it == values_.end()) {
            return std::nullopt;
        }

        return std::visit([](auto const& val) -> std::optional<float> {
            using T = std::decay_t<decltype(val)>;
            if constexpr (std::is_same_v<T, float>) {
                return val;
            } else if constexpr (std::is_same_v<T, int64_t>) {
                return static_cast<float>(val);
            } else {
                return std::nullopt;  // string cannot convert to float
            }
        }, it->second);
    }

    /**
     * @brief Get value as int64_t
     *
     * Performs type conversion if necessary:
     * - int64_t: returns directly
     * - float: truncates to int64_t
     * - string: returns nullopt
     *
     * @param key Key to look up
     * @return Int64 value, or std::nullopt if key not found or incompatible type
     */
    [[nodiscard]] std::optional<int64_t> getInt(std::string const& key) const {
        auto it = values_.find(key);
        if (it == values_.end()) {
            return std::nullopt;
        }

        return std::visit([](auto const& val) -> std::optional<int64_t> {
            using T = std::decay_t<decltype(val)>;
            if constexpr (std::is_same_v<T, int64_t>) {
                return val;
            } else if constexpr (std::is_same_v<T, float>) {
                return static_cast<int64_t>(val);
            } else {
                return std::nullopt;  // string cannot convert to int
            }
        }, it->second);
    }

    /**
     * @brief Get value as string
     *
     * Only returns a value if the stored type is string.
     * Does not perform numeric-to-string conversion.
     *
     * @param key Key to look up
     * @return String value, or std::nullopt if key not found or not a string
     */
    [[nodiscard]] std::optional<std::string> getString(std::string const& key) const {
        auto it = values_.find(key);
        if (it == values_.end()) {
            return std::nullopt;
        }

        if (auto const* str = std::get_if<std::string>(&it->second)) {
            return *str;
        }
        return std::nullopt;
    }

    /**
     * @brief Get the raw variant value
     *
     * @param key Key to look up
     * @return The stored variant, or std::nullopt if key not found
     */
    [[nodiscard]] std::optional<PipelineValue> get(std::string const& key) const {
        auto it = values_.find(key);
        if (it == values_.end()) {
            return std::nullopt;
        }
        return it->second;
    }

    // ========================================================================
    // Query Methods
    // ========================================================================

    /**
     * @brief Check if a key exists in the store
     * @param key Key to check
     * @return true if the key exists
     */
    [[nodiscard]] bool contains(std::string const& key) const {
        return values_.find(key) != values_.end();
    }

    /**
     * @brief Get the number of values in the store
     */
    [[nodiscard]] size_t size() const noexcept {
        return values_.size();
    }

    /**
     * @brief Check if the store is empty
     */
    [[nodiscard]] bool empty() const noexcept {
        return values_.empty();
    }

    /**
     * @brief Get all keys in the store
     * @return Vector of all keys
     */
    [[nodiscard]] std::vector<std::string> keys() const {
        std::vector<std::string> result;
        result.reserve(values_.size());
        for (auto const& [key, _] : values_) {
            result.push_back(key);
        }
        return result;
    }

    // ========================================================================
    // Mutation Methods
    // ========================================================================

    /**
     * @brief Merge another store into this one
     *
     * Values from the other store overwrite existing values with the same key.
     *
     * @param other Store to merge from
     */
    void merge(PipelineValueStore const& other) {
        for (auto const& [key, value] : other.values_) {
            values_[key] = value;
        }
    }

    /**
     * @brief Remove a value from the store
     * @param key Key to remove
     * @return true if the key was found and removed
     */
    bool erase(std::string const& key) {
        return values_.erase(key) > 0;
    }

    /**
     * @brief Clear all values from the store
     */
    void clear() noexcept {
        values_.clear();
    }

private:
    std::unordered_map<std::string, PipelineValue> values_;
};

}  // namespace WhiskerToolbox::Transforms::V2

#endif  // WHISKERTOOLBOX_V2_PIPELINE_VALUE_STORE_HPP
