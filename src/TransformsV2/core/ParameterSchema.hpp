#ifndef WHISKERTOOLBOX_V2_PARAMETER_SCHEMA_HPP
#define WHISKERTOOLBOX_V2_PARAMETER_SCHEMA_HPP

#include <rfl.hpp>
#include <rfl/json.hpp>

#include <optional>
#include <string>
#include <vector>

namespace WhiskerToolbox::Transforms::V2 {

// ============================================================================
// ParameterFieldDescriptor — Runtime schema for a single parameter field
// ============================================================================

/**
 * @brief Describes a single field in a parameter struct
 *
 * Created at compile-time via rfl::fields<T>() and augmented with
 * validator constraint parsing and optional UI hints.
 */
struct ParameterFieldDescriptor {
    std::string name;              ///< Field name from reflect-cpp (e.g., "scale_factor")
    std::string type_name;         ///< Underlying type: "float", "int", "std::string", "bool"
    std::string raw_type_name;     ///< Full raw type string from reflect-cpp
    std::string display_name;      ///< snake_case → "Title Case" for UI display

    // Value range (from rfl::Validator constraints or manual UI hints)
    std::optional<double> min_value;
    std::optional<double> max_value;
    bool is_exclusive_min = false;
    bool is_exclusive_max = false;

    // For enum-like string fields (e.g., {"positive", "negative", "absolute"})
    std::vector<std::string> allowed_values;

    // Default value as JSON string for type-erased storage
    std::optional<std::string> default_value_json;

    // UI hints
    std::string tooltip;
    std::string group;           ///< For grouping fields in the UI
    bool is_advanced = false;    ///< Collapsed by default in the UI
    int display_order = 0;

    bool is_optional = false;    ///< True for std::optional<T> fields
    bool is_bound = false;       ///< True if typically populated from PipelineValueStore
};

// ============================================================================
// ParameterSchema — Complete schema for a parameter struct
// ============================================================================

/**
 * @brief Runtime schema describing all fields in a parameter struct
 *
 * Built from rfl::fields<T>() at registration time with optional
 * ParameterUIHints<T> annotations applied.
 */
struct ParameterSchema {
    std::string params_type_name;
    std::vector<ParameterFieldDescriptor> fields;

    /// Find a mutable field descriptor by name (for annotation)
    ParameterFieldDescriptor * field(std::string const & name) {
        for (auto & f : fields) {
            if (f.name == name) return &f;
        }
        return nullptr;
    }

    /// Find a const field descriptor by name
    [[nodiscard]] ParameterFieldDescriptor const * field(std::string const & name) const {
        for (auto const & f : fields) {
            if (f.name == name) return &f;
        }
        return nullptr;
    }
};

// ============================================================================
// ParameterUIHints — Optional override trait for richer UI
// ============================================================================

/**
 * @brief Trait struct for providing custom UI hints for a parameter type
 *
 * Specialize this template to annotate fields with tooltips, allowed values,
 * groups, default values, etc. The default (unspecialized) version does nothing.
 *
 * Example:
 * @code
 * template<>
 * struct ParameterUIHints<LineAngleParams> {
 *     static void annotate(ParameterSchema& schema) {
 *         schema.field("position")->tooltip = "Fractional position (0.0 = start, 1.0 = end)";
 *         schema.field("method")->allowed_values = {"DirectPoints", "PolynomialFit"};
 *     }
 * };
 * @endcode
 */
template<typename Params>
struct ParameterUIHints {
    static void annotate(ParameterSchema & /*schema*/) {
        // Default: no annotations
    }
};

// ============================================================================
// Helper Functions (declared here, defined in ParameterSchema.cpp)
// ============================================================================

/// Convert snake_case field name to "Title Case" for display
std::string snakeCaseToDisplay(std::string const & snake);

/// Parse a reflect-cpp type string to determine the underlying type name
/// e.g., "std::optional<rfl::Validator<float, ...>>" → "float"
std::string parseUnderlyingType(std::string const & type_str);

/// Check if a type string represents an optional field
bool isOptionalType(std::string const & type_str);

/// Check if a type string contains an rfl::Validator
bool hasValidator(std::string const & type_str);

/// Constraint information extracted from a type string
struct ConstraintInfo {
    std::optional<double> min_value;
    std::optional<double> max_value;
    bool is_exclusive_min = false;
    bool is_exclusive_max = false;
};

/// Extract validator constraint information from a type string
ConstraintInfo extractConstraints(std::string const & type_str);

// ============================================================================
// Compile-Time Schema Extraction
// ============================================================================

/**
 * @brief Extract a ParameterSchema from a reflect-cpp parameter struct
 *
 * Uses rfl::fields<T>() to enumerate fields and parse type information
 * from the type strings. Applies ParameterUIHints<T> if a specialization exists.
 *
 * This is typically called once per parameter type at registration time.
 *
 * @tparam Params The parameter struct type (must be reflectable by reflect-cpp)
 * @return ParameterSchema describing all fields in the struct
 */
template<typename Params>
ParameterSchema extractParameterSchema() {
    ParameterSchema schema;
    schema.params_type_name = typeid(Params).name();

    auto meta_fields = rfl::fields<Params>();

    int order = 0;
    for (auto const & mf : meta_fields) {
        ParameterFieldDescriptor desc;
        desc.name = mf.name();
        desc.raw_type_name = mf.type();
        desc.type_name = parseUnderlyingType(mf.type());
        desc.display_name = snakeCaseToDisplay(mf.name());
        desc.is_optional = isOptionalType(mf.type());
        desc.display_order = order++;

        // Extract validator constraints if present
        if (hasValidator(mf.type())) {
            auto constraints = extractConstraints(mf.type());
            desc.min_value = constraints.min_value;
            desc.max_value = constraints.max_value;
            desc.is_exclusive_min = constraints.is_exclusive_min;
            desc.is_exclusive_max = constraints.is_exclusive_max;
        }

        schema.fields.push_back(std::move(desc));
    }

    // Apply optional UI hints from specializations
    ParameterUIHints<Params>::annotate(schema);

    return schema;
}

}// namespace WhiskerToolbox::Transforms::V2

#endif// WHISKERTOOLBOX_V2_PARAMETER_SCHEMA_HPP
