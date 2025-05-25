#ifndef TRANSFORMATIONSCOMMON_HPP
#define TRANSFORMATIONSCOMMON_HPP

#include <optional>
#include <string> // For std::string in potential future param maps
#include <variant> // Added
// #include <QVariant> // If QVariant is ever needed for parameters, but avoid Qt here if possible

namespace WhiskerTransformations {

enum class TransformationType {
    Identity,
    Squared,
    LagLead
};

// Parameter Structs
struct IdentityParams { // Empty for now
    bool operator==(const IdentityParams&) const { return true; }
    bool operator!=(const IdentityParams&) const { return false; }
};

struct SquaredParams { // Empty for now
    bool operator==(const SquaredParams&) const { return true; }
    bool operator!=(const SquaredParams&) const { return false; }
};

struct LagLeadParams {
    int min_lag_steps = 0;
    int max_lead_steps = 0;
    bool operator==(const LagLeadParams& other) const {
        return min_lag_steps == other.min_lag_steps && max_lead_steps == other.max_lead_steps;
    }
    // Add a inequality operator for std::optional comparison
    bool operator!=(const LagLeadParams& other) const {
        return !(*this == other);
    }
};

// Variant to hold any of the parameter structs
using ParametersVariant = std::variant<
    IdentityParams,
    SquaredParams,
    LagLeadParams
    // Add future parameter structs here
>;

struct AppliedTransformation {
    TransformationType type;
    ParametersVariant params; // Changed from std::optional<LagLeadParams>

    // Note: Implementing general == and != for variant-holding structs can be tricky
    // if the variant itself doesn't have a trivial comparison. 
    // For now, assume it's needed and handle appropriately or simplify if exact comparison isn't critical.
    // A simple approach for == might be to check type and then compare specific params if type matches.
    bool operator==(const AppliedTransformation& other) const {
        if (type != other.type) return false;
        // Compare params based on type - this requires a bit more work or specific knowledge
        // For simplicity now, we might assume if types are same, variant comparison works if underlying types are comparable
        return params == other.params; // This works if all types in variant are comparable.
    }
    bool operator!=(const AppliedTransformation& other) const {
        return !(*this == other);
    }
};

struct ProcessedFeatureInfo {
    std::string base_feature_key;
    AppliedTransformation transformation;
    std::string output_feature_name;
};

} // namespace WhiskerTransformations

#endif // TRANSFORMATIONSCOMMON_HPP 