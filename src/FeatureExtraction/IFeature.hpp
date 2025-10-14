#ifndef FEATURE_EXTRACTION_IFEATURE_HPP
#define FEATURE_EXTRACTION_IFEATURE_HPP

#include "CoreGeometry/lines.hpp"
#include "CoreGeometry/points.hpp"


#include <memory>
#include <stdexcept>
#include <string>
#include <typeindex>
#include <variant>
#include <vector>


// A variant to hold pointers to all possible feature INPUT types.
// This is a type-safe alternative to std::any. When a new data type
// (e.g., Mask) needs features, add its const pointer type here.
using FeatureInputVariant = std::variant<
        Line2D const *,
        Point2D<float> const *
        // const MaskData* // Example for the future
        >;

// A variant to hold possible feature RETURN types.
// This can be expanded as new feature output types are needed.
using FeatureResultVariant = std::variant<
        double,
        float,
        int,
        Point2D<float>>;

/**
 * @brief The core abstract base class for any computable feature.
 * * This interface defines a "single source of truth" for a feature, containing
 * all necessary metadata and the computation logic itself.
 */
class IFeature {
public:
    virtual ~IFeature() = default;

    // --- Metadata ---
    virtual std::string getName() const = 0;       // Internal name (e.g., "line_length")
    virtual std::string getUIName() const = 0;     // Display name (e.g., "Line Length")
    virtual std::string getDescription() const = 0;// Tooltip/help text

    // --- Type Information ---
    virtual std::type_index getInputType() const = 0;
    virtual std::type_index getOutputType() const = 0;

    // --- Computation ---
    // Takes a variant of pointers to allow for type-safe generic dispatch.
    virtual FeatureResultVariant compute(FeatureInputVariant input) const = 0;

    // --- Parameter Handling (for UI and JSON) ---
    //virtual void setParameters(nlohmann::json const & params) = 0;
};

/**
 * @brief A templated helper class to simplify feature implementation.
 * * Inherit from this class to get automatic type handling for input and output.
 * You only need to implement the `computeTyped` method and the metadata.
 * * @tparam T_in The specific input data type (e.g., Line2D).
 * @tparam T_out The specific output data type (e.g., double).
 */
template<typename T_in, typename T_out>
class FeatureBase : public IFeature {
public:
    // Automatically provide type information based on template parameters.
    std::type_index getInputType() const final { return typeid(T_in); }
    std::type_index getOutputType() const final { return typeid(T_out); }

    // Handles the variant visiting and dispatches to the correct typed implementation.
    FeatureResultVariant compute(FeatureInputVariant input) const final {
        // Use std::visit to safely operate on the variant.
        return std::visit([this](auto * in_ptr) -> FeatureResultVariant {
            // Check if the pointer in the variant is of the correct type.
            if constexpr (std::is_same_v<decltype(in_ptr), T_in const *>) {
                if (in_ptr) {
                    return this->computeTyped(*in_ptr);
                }
            }
            // This is a developer error (e.g., passing a nullptr or a variant
            // that was not handled in the adapter), so throwing is appropriate.
            throw std::runtime_error("Invalid input type or null pointer for feature: " + this->getName());
        },
                          input);
    }

    // The method that concrete feature classes must implement.
    virtual T_out computeTyped(T_in const & input) const = 0;
};

#endif// FEATURE_EXTRACTION_IFEATURE_HPP
