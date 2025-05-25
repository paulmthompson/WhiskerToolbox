#include "IdentityTransform.hpp"

arma::Mat<double> IdentityTransform::_applyTransformationLogic(
    const arma::Mat<double>& base_data,
    const WhiskerTransformations::AppliedTransformation& transform_config,
    std::string& error_message) const 
{
    // For Identity, no actual transformation is applied to the fetched data.
    // transform_config is ignored for Identity.
    return base_data;
} 