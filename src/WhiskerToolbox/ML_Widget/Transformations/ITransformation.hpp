#ifndef ITRANSFORMATION_HPP
#define ITRANSFORMATION_HPP

#include "DataManager/DataManagerFwd.hpp"//DM_DataType
#include "TransformationsCommon.hpp"

#include <armadillo>

#include <string>
#include <vector>

class DataManager;

class ITransformation {
public:
    virtual ~ITransformation() = default;

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
    virtual arma::Mat<double> apply(
            DataManager * dm,
            std::string const & base_key,
            DM_DataType data_type,
            std::vector<std::size_t> const & timestamps,
            WhiskerTransformations::AppliedTransformation const & transform_config,
            std::string & error_message) const = 0;

    /**
     * @brief Checks if this transformation can be applied to the given data type.
     * @param type The DM_DataType to check.
     * @return True if the transformation supports this data type, false otherwise.
     */
    [[nodiscard]] virtual bool isSupported(DM_DataType type) const = 0;
};

#endif//ITRANSFORMATION_HPP
