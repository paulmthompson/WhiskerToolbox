#ifndef IDENTITYTRANSFORM_HPP
#define IDENTITYTRANSFORM_HPP

#include "TransformationBase.hpp"

class IdentityTransform : public TransformationBase {
public:
    arma::Mat<double> _applyTransformationLogic(
        const arma::Mat<double>& base_data,
        const WhiskerTransformations::AppliedTransformation& transform_config,
        std::string& error_message) const override;

    // isSupported can be inherited from TransformationBase if it's general enough,
    // or overridden if Identity has specific data type constraints.
    // For now, we'll assume base class isSupported is fine.
};

#endif //IDENTITYTRANSFORM_HPP 