#ifndef SQUAREDTRANSFORM_HPP
#define SQUAREDTRANSFORM_HPP

#include "TransformationBase.hpp"

class SquaredTransform : public TransformationBase {
public:
    arma::Mat<double> _applyTransformationLogic(
            arma::Mat<double> const & base_data,
            WhiskerTransformations::AppliedTransformation const & transform_config,
            std::string & error_message) const override;

    /**
     * @brief Checks if this transformation can be applied to the given data type.
     * @param type The DM_DataType to check.
     * @return True if the transformation supports this data type, false otherwise.
     *
     * @note Supports Analog, Points, and Tensor data types.
     */
    bool isSupported(DM_DataType type) const override;
};

#endif//SQUAREDTRANSFORM_HPP