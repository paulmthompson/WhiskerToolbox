#include "SquaredTransform.hpp"

arma::Mat<double> SquaredTransform::_applyTransformationLogic(
        arma::Mat<double> const & base_data,
        WhiskerTransformations::AppliedTransformation const & transform_config,
        std::string & error_message) const {

    static_cast<void>(transform_config);

    if (base_data.empty()) {
        error_message = "Base data for SquaredTransform is empty.";
        return arma::Mat<double>();
    }
    // transform_config is ignored for Squared.
    return arma::pow(base_data, 2);
}

bool SquaredTransform::isSupported(DM_DataType type) const {
    return type == DM_DataType::Analog ||
           type == DM_DataType::Points ||
           type == DM_DataType::Tensor;
}
