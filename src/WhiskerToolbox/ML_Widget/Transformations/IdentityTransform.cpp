#include "IdentityTransform.hpp"

arma::Mat<double> IdentityTransform::_applyTransformationLogic(
        arma::Mat<double> const & base_data,
        WhiskerTransformations::AppliedTransformation const & transform_config,
        std::string & error_message) const {

    static_cast<void>(transform_config);
    static_cast<void>(error_message);

    return base_data;
}
