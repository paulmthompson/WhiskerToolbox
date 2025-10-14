#ifndef FEATURE_EXTRACTION_FEATURE_VECTOR_HPP
#define FEATURE_EXTRACTION_FEATURE_VECTOR_HPP

#include "CoreGeometry/points.hpp"
#include "IFeature.hpp"

#include <string>
#include <variant>
#include <vector>

/**
 * @brief A single, self-describing feature value.
 * It holds the name of the feature and its value in a type-safe variant.
 */
struct FeatureValue {
    // Using string_view is a major optimization. It avoids string allocations
    // by holding a non-owning view of the feature's name from the IFeature object.
    // This assumes the IFeature object outlives this FeatureValue.
    std::string_view name;

    // The actual computed value, stored in the same type-safe variant
    // used by the IFeature interface.
    FeatureResultVariant value;
};


/**
 * @brief A type-safe, heterogeneous container for the results of one or more feature extractions.
 *
 * This class stores a collection of FeatureValue structs, allowing it to hold
 * different types (doubles, Points, etc.) without flattening them into a single primitive type.
 * Each value is paired with a name, making the vector self-describing and ideal for
 * serialization, debugging, and UI display, while also providing efficient columnar access.
 */
class FeatureVector {
public:
    FeatureVector() = default;

    /**
     * @brief Appends a feature result, preserving its structure.
     * This overload takes the full feature to access its name metadata.
     */
    void append(IFeature const & feature, FeatureResultVariant && value) {
        values_.emplace_back(FeatureValue{feature.getName(), std::move(value)});
    }

    /**
     * @brief Concatenates another FeatureVector onto this one.
     */
    void append(FeatureVector const & other) {
        values_.insert(values_.end(), other.values_.begin(), other.values_.end());
    }

    /**
     * @brief Extracts all values of a specific type associated with a feature name.
     * This is the primary method for converting feature results into a columnar format.
     *
     * @example auto lengths = vec.getValuesByName<double>("line_length");
     * @tparam T The desired type to extract (e.g., double, Point2D<float>).
     * @param feature_name The name of the feature to search for.
     * @return A std::vector containing only the values of the requested type and name.
     */
    template<typename T>
    std::vector<T> getValuesByName(std::string_view feature_name) const {
        std::vector<T> result;
        for (auto const & fv: values_) {
            if (fv.name == feature_name) {
                if (T const * val = std::get_if<T>(&fv.value)) {
                    result.push_back(*val);
                }
            }
        }
        return result;
    }

    /**
     * @brief Converts the entire vector into a flat vector of doubles.
     *
     * This is intended for backwards compatibility with systems that expect
     * flattened data structures (e.g., Kalman filters). It decomposes complex
     * types like Point2D into their constituent float/double values.
     *
     * @return A std::vector<double> containing all numerical data.
     */
    std::vector<double> toFlatDoubleVector() const {
        std::vector<double> flat_vector;
        // Reserve a reasonable amount of memory to reduce reallocations.
        flat_vector.reserve(values_.size());

        for (auto const & fv: values_) {
            std::visit([&flat_vector](auto && arg) {
                using T = std::decay_t<decltype(arg)>;
                if constexpr (std::is_same_v<T, Point2D<float>>) {
                    flat_vector.push_back(static_cast<double>(arg.x));
                    flat_vector.push_back(static_cast<double>(arg.y));
                } else if constexpr (std::is_constructible_v<double, T>) {
                    // This handles double, float, int, etc.
                    flat_vector.push_back(static_cast<double>(arg));
                }
                // Non-convertible types are silently ignored in the flattening process.
            },
                       fv.value);
        }
        return flat_vector;
    }


    // --- Accessors ---
    std::vector<FeatureValue> const & getValues() const { return values_; }
    size_t size() const { return values_.size(); }
    bool empty() const { return values_.empty(); }
    void clear() { values_.clear(); }

    // --- Iterators to allow for range-based for loops ---
    auto begin() const { return values_.begin(); }
    auto end() const { return values_.end(); }
    auto begin() { return values_.begin(); }
    auto end() { return values_.end(); }

private:
    std::vector<FeatureValue> values_{};
};

#endif// FEATURE_EXTRACTION_FEATURE_VECTOR_HPP