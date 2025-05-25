#include "SquaredTransform.hpp"

arma::Mat<double> SquaredTransform::_applyTransformationLogic(
    const arma::Mat<double>& base_data,
    const WhiskerTransformations::AppliedTransformation& transform_config,
    std::string& error_message) const 
{
    if (base_data.empty()) {
        error_message = "Base data for SquaredTransform is empty.";
        return arma::Mat<double>();
    }
    // transform_config is ignored for Squared.
    return arma::pow(base_data, 2);
} 