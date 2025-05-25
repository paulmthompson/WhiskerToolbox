#ifndef TRANSFORMATIONBASE_HPP
#define TRANSFORMATIONBASE_HPP

#include "ITransformation.hpp"
// #include "ML_Widget/mlpack_conversion.hpp" // Correct path might be needed depending on include dirs
// For now, assume mlpack_conversion.hpp is accessible, or TransformationBase.cpp includes it directly where needed.

class TransformationBase : public ITransformation {
public:
    virtual ~TransformationBase() = default;

    // Common method to fetch data, to be called by derived classes
    // This method returns the data in the desired format (features as rows, samples as columns)
    arma::Mat<double> fetchBaseData(
        DataManager* dm,
        const std::string& base_key,
        DM_DataType data_type,
        const std::vector<std::size_t>& timestamps,
        std::string& error_message) const;

    // Default implementation for isSupported - can be overridden by specific transforms
    // By default, supports Analog, DigitalInterval, Points, Tensor
    bool isSupported(DM_DataType type) const override;

    arma::Mat<double> apply(
            DataManager* dm,
            const std::string& base_key,
            DM_DataType data_type,
            const std::vector<std::size_t>& timestamps,
            const WhiskerTransformations::AppliedTransformation& transform_config,
            std::string& error_message) const override;
protected:
    // Helper to apply the core transformation logic on already fetched data
    // Derived classes will override this to perform their specific mathematical operation.
    virtual arma::Mat<double> _applyTransformationLogic( 
        const arma::Mat<double>& base_data,
        const WhiskerTransformations::AppliedTransformation& transform_config,
        std::string& error_message) const = 0;
};

#endif //TRANSFORMATIONBASE_HPP 
