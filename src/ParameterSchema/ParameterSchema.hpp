/**
 * @file ParameterSchema.hpp
 * @brief Compile-time parameter schema extraction for reflect-cpp structs.
 *
 * Provides ParameterFieldDescriptor, ParameterSchema, extractParameterSchema<T>(),
 * and ParameterUIHints<T> for use by TransformsV2 and the Command Architecture.
 * No Qt dependency; depends only on reflect-cpp and the standard library.
 */
#ifndef WHISKERTOOLBOX_V2_PARAMETER_SCHEMA_HPP
#define WHISKERTOOLBOX_V2_PARAMETER_SCHEMA_HPP

#include <rfl.hpp>
#include <rfl/json.hpp>

#include "TimeFrame/TimeFrameIndexReflector.hpp"

#include <memory>
#include <optional>
#include <string>
#include <type_traits>
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

// Forward declarations to break circular dependency:
// VariantAlternative → ParameterSchema → ParameterFieldDescriptor → VariantAlternative
struct ParameterSchema;

/**
 * @brief Describes one alternative in a tagged-union variant field
 *
 * Each alternative has a tag string (the discriminator value) and a
 * sub-schema describing the fields for that alternative's struct.
 * Uses unique_ptr to break the circular type dependency.
 */
struct VariantAlternative {
    std::string tag;                        ///< Discriminator value (e.g., "LinearMotionParams")
    std::unique_ptr<ParameterSchema> schema;///< Sub-schema for this alternative's fields

    VariantAlternative();
    ~VariantAlternative();
    VariantAlternative(VariantAlternative const & other);
    VariantAlternative & operator=(VariantAlternative const & other);
    VariantAlternative(VariantAlternative && other) noexcept;
    VariantAlternative & operator=(VariantAlternative && other) noexcept;
};

struct ParameterFieldDescriptor {
    std::string name;         ///< Field name from reflect-cpp (e.g., "scale_factor")
    std::string type_name;    ///< Underlying type: "float", "int", "std::string", "bool", "enum", "variant"
    std::string raw_type_name;///< Full raw type string from reflect-cpp
    std::string display_name; ///< snake_case → "Title Case" for UI display

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
    std::string group;       ///< For grouping fields in the UI
    bool is_advanced = false;///< Collapsed by default in the UI
    int display_order = 0;

    bool is_optional = false;///< True for std::optional<T> fields
    bool is_bound = false;   ///< True if typically populated from PipelineValueStore

    // Dynamic combo support (for fields populated at runtime, e.g. DataManager keys)
    bool dynamic_combo = false;        ///< If true, create QComboBox even with empty allowed_values
    bool include_none_sentinel = false;///< If true, prepend "(None)" sentinel to combo

    // Variant support (populated only when type_name == "variant")
    bool is_variant = false;                             ///< True for rfl::TaggedUnion fields
    std::string variant_discriminator;                   ///< Discriminator field name (e.g., "model")
    std::vector<VariantAlternative> variant_alternatives;///< One entry per union alternative
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
        for (auto & f: fields) {
            if (f.name == name) return &f;
        }
        return nullptr;
    }

    /// Find a const field descriptor by name
    [[nodiscard]] ParameterFieldDescriptor const * field(std::string const & name) const {
        for (auto const & f: fields) {
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
// Compile-Time Helpers
// ============================================================================

namespace detail {

/// Unwrap std::optional<T> to T; identity for non-optional types.
template<typename T>
struct unwrap_optional {
    using type = T;
};

template<typename T>
struct unwrap_optional<std::optional<T>> {
    using type = T;
};

template<typename T>
using unwrap_optional_t = typename unwrap_optional<T>::type;

/// Detect rfl::TaggedUnion<_discriminator, Ts...> via SFINAE
template<typename T, typename = void>
struct is_tagged_union : std::false_type {};

template<typename T>
struct is_tagged_union<T, std::void_t<
                                  typename T::VariantType,
                                  decltype(T::discrimininator_)>> : std::true_type {};

template<typename T>
inline constexpr bool is_tagged_union_v = is_tagged_union<T>::value;

/// Extract the first alternative type from a TaggedUnion (for default construction).
/// TaggedUnion has no default constructor; we use the first alternative.
/// Disc is a non-type template parameter (StringLiteral), so use auto.
template<typename T>
struct first_tagged_union_alt {};

template<auto Disc, typename First, typename... Rest>
struct first_tagged_union_alt<rfl::TaggedUnion<Disc, First, Rest...>> {
    using type = First;
};

/// Return a default instance of Params for schema extraction.
/// TaggedUnion is not default-constructible; use first alternative.
template<typename Params>
Params getDefaultInstance() {
    if constexpr (is_tagged_union_v<Params>) {
        using FirstAlt = typename first_tagged_union_alt<Params>::type;
        return Params{FirstAlt{}};
    } else {
        return Params{};
    }
}

}// namespace detail

// Forward declaration for recursive use in detail::makeVariantAlternative
template<typename Params>
ParameterSchema extractParameterSchema();

namespace detail {

/// @brief Process a single alternative type in a TaggedUnion.
///
/// Extracts the tag name by serializing a default-constructed TaggedUnion holding
/// the alternative, and recursively extracts the sub-schema for that alternative.
template<typename TaggedUnionType, typename AltType>
VariantAlternative makeVariantAlternative(std::string_view disc_str) {
    VariantAlternative alt;
    // Extract tag name: serialize a TaggedUnion holding a default-constructed AltType
    TaggedUnionType tu{AltType{}};
    auto json_str = rfl::json::write(tu);
    auto parsed = rfl::json::read<rfl::Generic>(json_str);
    if (parsed) {
        auto const * obj = std::get_if<rfl::Generic::Object>(&parsed.value().get());
        if (obj) {
            auto disc_val = obj->get(std::string(disc_str));
            if (disc_val) {
                if (auto const * s = std::get_if<std::string>(&disc_val.value().get())) {
                    alt.tag = *s;
                }
            }
        }
    }
    // Recursively extract sub-schema for the alternative struct
    *alt.schema = extractParameterSchema<AltType>();
    return alt;
}

/// @brief Process all alternatives in a TaggedUnion and populate the field descriptor.
template<rfl::internal::StringLiteral _disc, typename... Ts>
void processTaggedUnion(
        rfl::TaggedUnion<_disc, Ts...> const * /*type_tag*/,
        ParameterFieldDescriptor & desc) {
    using TU = rfl::TaggedUnion<_disc, Ts...>;
    constexpr auto disc_sv = _disc.string_view();
    desc.is_variant = true;
    desc.type_name = "variant";
    desc.variant_discriminator = std::string(disc_sv);
    desc.variant_alternatives.clear();
    desc.allowed_values.clear();

    // Fold expression: process each alternative type
    (([&] {
         auto alt = makeVariantAlternative<TU, Ts>(disc_sv);
         desc.allowed_values.push_back(alt.tag);
         desc.variant_alternatives.push_back(std::move(alt));
     }()),
     ...);
}

}// namespace detail

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

    // Default-construct a Params instance and serialize to JSON to extract defaults.
    // TaggedUnion has no default constructor; use first alternative.
    auto const defaults_instance = detail::getDefaultInstance<Params>();
    auto const defaults_json = rfl::json::write(defaults_instance);
    auto const defaults_parsed = rfl::json::read<rfl::Generic>(defaults_json);
    rfl::Generic::Object const * defaults_obj = nullptr;
    if (defaults_parsed) {
        defaults_obj = std::get_if<rfl::Generic::Object>(&defaults_parsed.value().get());
    }

    int order = 0;
    for (auto const & mf: meta_fields) {
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

        // Extract default value from the default-constructed instance
        if (defaults_obj) {
            auto field_val = defaults_obj->get(desc.name);
            if (field_val) {
                desc.default_value_json = rfl::json::write(field_val.value());
            }
        }

        schema.fields.push_back(std::move(desc));
    }

    // Enum detection pass: rfl::fields() only provides type strings, but we
    // need actual C++ types to use std::is_enum_v and rfl::get_enumerator_array.
    // rfl::to_view() gives typed references to each field.
    auto view = rfl::to_view(defaults_instance);
    size_t field_idx = 0;
    view.apply([&](auto const & field) {
        // rfl::to_view returns pointer fields, so field.value() is T*
        using PtrType = std::remove_cvref_t<decltype(field.value())>;
        using RawFieldType = std::remove_pointer_t<PtrType>;
        using InnerType = detail::unwrap_optional_t<RawFieldType>;

        if constexpr (detail::is_tagged_union_v<InnerType>) {
            if (field_idx < schema.fields.size()) {
                detail::processTaggedUnion(
                        static_cast<InnerType const *>(nullptr),
                        schema.fields[field_idx]);
            }
        } else if constexpr (std::is_enum_v<InnerType>) {
            if (field_idx < schema.fields.size()) {
                auto & fd = schema.fields[field_idx];
                fd.type_name = "enum";
                fd.allowed_values.clear();
                constexpr auto enumerators = rfl::get_enumerator_array<InnerType>();
                for (auto const & [name, val]: enumerators) {
                    fd.allowed_values.emplace_back(std::string(name));
                }
            }
        }
        ++field_idx;
    });

    // Apply optional UI hints from specializations
    ParameterUIHints<Params>::annotate(schema);

    return schema;
}

}// namespace WhiskerToolbox::Transforms::V2

#endif// WHISKERTOOLBOX_V2_PARAMETER_SCHEMA_HPP
