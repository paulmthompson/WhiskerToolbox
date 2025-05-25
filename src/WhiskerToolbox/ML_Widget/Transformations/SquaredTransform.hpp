#ifndef SQUAREDTRANSFORM_HPP
#define SQUAREDTRANSFORM_HPP

#include "TransformationBase.hpp"

class SquaredTransform : public TransformationBase {
public:
    arma::Mat<double> _applyTransformationLogic(
        const arma::Mat<double>& base_data,
        const WhiskerTransformations::AppliedTransformation& transform_config,
        std::string& error_message) const override;

    // isSupported can be inherited from TransformationBase.
    // No need to override if it supports all types TransformationBase does.
};

#endif //SQUAREDTRANSFORM_HPP 