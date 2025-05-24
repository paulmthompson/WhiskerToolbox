#include "IdentityTransform.hpp"

arma::Mat<double> IdentityTransform::_applyTransformationLogic(
    const arma::Mat<double>& base_data,
    std::string& error_message) const 
{
    // For Identity, no actual transformation is applied to the fetched data.
    // The base_data is already in the correct (features x samples) format.
    // error_message remains unchanged unless there's a specific error for Identity itself.
    return base_data;
} 