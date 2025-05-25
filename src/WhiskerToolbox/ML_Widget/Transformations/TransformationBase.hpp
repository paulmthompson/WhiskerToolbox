#ifndef TRANSFORMATIONBASE_HPP
#define TRANSFORMATIONBASE_HPP

#include "ITransformation.hpp"

class TransformationBase : public ITransformation {
public:
    virtual ~TransformationBase() = default;

    /**
     * @brief Fetches the base data from the DataManager.
     * @param dm Pointer to DataManager.
     * @param base_key The key for the base data series.
     * @param data_type The DM_DataType of the base_key.
     * @param timestamps The timestamps for which to fetch data.
     * @param error_message Output string for any errors encountered.
     * @return arma::Mat<double> The fetched data matrix (features as rows, samples as columns). Empty if error.
     */
    arma::Mat<double> fetchBaseData(
            DataManager * dm,
            std::string const & base_key,
            DM_DataType data_type,
            std::vector<std::size_t> const & timestamps,
            std::string & error_message) const;

    /**
     * @brief Checks if this transformation can be applied to the given data type.
     * @param type The DM_DataType to check.
     * @return True if the transformation supports this data type, false otherwise.
     *
     * @note This is a default implementation.
     * Derived classes can override this method to specify their own supported data types.
     * Default behavior is to support Analog, DigitalInterval, Points, and Tensor data types.
     */
    [[nodiscard]] bool isSupported(DM_DataType type) const override;

    /**
     * @brief Applies the transformation.
     * Fetches data based on base_key and data_type, applies transformation logic.
     * @param dm Pointer to DataManager.
     * @param base_key The key for the base data series.
     * @param data_type The DM_DataType of the base_key.
     * @param timestamps The timestamps for which to fetch and process data.
     * @param transform_config The configuration for the transformation.
     * @param error_message Output string for any errors encountered.
     * @return arma::Mat<double> The transformed data matrix (features as rows, samples as columns). Empty if error.
     */
    arma::Mat<double> apply(
            DataManager * dm,
            std::string const & base_key,
            DM_DataType data_type,
            std::vector<std::size_t> const & timestamps,
            WhiskerTransformations::AppliedTransformation const & transform_config,
            std::string & error_message) const override;

protected:

    /**
     * @brief Applies the transformation logic specific to the derived class.
     * @param base_data The base data matrix (features as rows, samples as columns).
     * @param transform_config The configuration for the transformation.
     * @param error_message Output string for any errors encountered.
     * @return arma::Mat<double> The transformed data matrix (features as rows, samples as columns). Empty if error.
     */
    virtual arma::Mat<double> _applyTransformationLogic(
            arma::Mat<double> const & base_data,
            WhiskerTransformations::AppliedTransformation const & transform_config,
            std::string & error_message) const = 0;
};

#endif//TRANSFORMATIONBASE_HPP
