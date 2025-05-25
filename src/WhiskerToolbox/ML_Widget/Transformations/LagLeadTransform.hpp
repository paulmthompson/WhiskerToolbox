#ifndef LAGLEADTRANSFORM_HPP
#define LAGLEADTRANSFORM_HPP

#include "TransformationBase.hpp"

class LagLeadTransform : public TransformationBase {
public:
    arma::Mat<double> _applyTransformationLogic(
        const arma::Mat<double>& base_data,
        const WhiskerTransformations::AppliedTransformation& transform_config,
        std::string& error_message) const override;

    // Optionally override isSupported if Lag/Lead has specific constraints
    // bool isSupported(DM_DataType type) const override;
};

#endif //LAGLEADTRANSFORM_HPP 