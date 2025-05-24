#include "SquaredTransform.hpp"

arma::Mat<double> SquaredTransform::_applyTransformationLogic(
    const arma::Mat<double>& base_data,
    std::string& error_message) const 
{
    if (base_data.empty()) {
        error_message = "Base data for SquaredTransform is empty.";
        return arma::Mat<double>();
    }
    // Apply the squaring transformation element-wise
    return arma::pow(base_data, 2);
} 