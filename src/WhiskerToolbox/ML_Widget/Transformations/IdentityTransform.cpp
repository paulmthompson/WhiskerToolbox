#include "IdentityTransform.hpp"

arma::Mat<double> IdentityTransform::_applyTransformationLogic(
        arma::Mat<double> const & base_data,
        WhiskerTransformations::AppliedTransformation const & transform_config,
        std::string & error_message) const {
    return base_data;
}