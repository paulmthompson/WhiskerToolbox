#ifndef LAGLEADTRANSFORM_HPP
#define LAGLEADTRANSFORM_HPP

#include "TransformationBase.hpp"

class LagLeadTransform : public TransformationBase {
public:
    arma::Mat<double> _applyTransformationLogic(
        const arma::Mat<double>& base_data,
        const WhiskerTransformations::AppliedTransformation& transform_config,
        std::string& error_message) const override;

    /**
     * @brief Checks if this transformation can be applied to the given data type.
     * @param type The DM_DataType to check.
     * @return True if the transformation supports this data type, false otherwise.
     *
     * @note Supports Analog, Points, and Tensor data types.
     */
    bool isSupported(DM_DataType type) const override;
};

#endif //LAGLEADTRANSFORM_HPP 