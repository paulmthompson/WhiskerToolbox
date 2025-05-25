#ifndef IDENTITYTRANSFORM_HPP
#define IDENTITYTRANSFORM_HPP

#include "TransformationBase.hpp"

/**
 * @brief IdentityTransform class is a no-op transformation.
 * This class applies an identity transformation to the input data.
 */
class IdentityTransform : public TransformationBase {
public:
    arma::Mat<double> _applyTransformationLogic(
        const arma::Mat<double>& base_data,
        const WhiskerTransformations::AppliedTransformation& transform_config,
        std::string& error_message) const override;

};

#endif //IDENTITYTRANSFORM_HPP 