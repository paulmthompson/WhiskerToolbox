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


struct IdentityParams {

};

struct SquaredParams {

};

struct LagLeadParams {
    int min_lag_steps = 0;
    int max_lead_steps = 0;
};

using ParametersVariant = std::variant<
    IdentityParams,
    SquaredParams,
    LagLeadParams
>;

struct AppliedTransformation {
    TransformationType type;
    ParametersVariant params;
};

struct ProcessedFeatureInfo {
    std::string base_feature_key;
    AppliedTransformation transformation;
    std::string output_feature_name;
};

} // namespace WhiskerTransformations

#endif // TRANSFORMATIONSCOMMON_HPP 
