#include "TransformationBase.hpp"

#include "DataManager/AnalogTimeSeries/Analog_Time_Series.hpp"
#include "DataManager/DataManager.hpp"
#include "DataManager/DigitalTimeSeries/Digital_Interval_Series.hpp"
#include "DataManager/Points/Point_Data.hpp"
#include "DataManager/Tensors/Tensor_Data.hpp"

#include "DataManager/utils/armadillo_wrap/analog_armadillo.hpp"
#include "ML_Widget/mlpack_conversion.hpp"

arma::Mat<double> TransformationBase::fetchBaseData(
        DataManager * dm,
        std::string const & base_key,
        DM_DataType data_type,
        std::vector<std::size_t> const & timestamps,
        std::string & error_message) const {
    arma::Mat<double> base_data_matrix;
    if (!dm) {
        error_message = "DataManager pointer is null.";
        return base_data_matrix;
    }

    if (timestamps.empty()) {
        error_message = "Timestamps vector is empty.";
        return base_data_matrix;
    }

    if (data_type == DM_DataType::Analog) {
        auto analog_series = dm->getData<AnalogTimeSeries>(base_key);
        if (analog_series) {
            base_data_matrix = convertAnalogTimeSeriesToMlpackArray(analog_series.get(), timestamps);
        }
    } else if (data_type == DM_DataType::DigitalInterval) {
        auto digital_series = dm->getData<DigitalIntervalSeries>(base_key);
        if (digital_series) {
            base_data_matrix = convertToMlpackArray(digital_series.get(), timestamps);
        }
    } else if (data_type == DM_DataType::Points) {
        auto point_data = dm->getData<PointData>(base_key);
        if (point_data) {
            base_data_matrix = convertToMlpackMatrix(point_data.get(), timestamps);
        }
    } else if (data_type == DM_DataType::Tensor) {
        auto tensor_data = dm->getData<TensorData>(base_key);
        if (tensor_data) {
            base_data_matrix = convertTensorDataToMlpackMatrix(tensor_data.get(), timestamps);
        }
    } else {
        error_message += "Unsupported data type '" + convert_data_type_to_string(data_type) + "' for feature '" + base_key + "'.\n";
    }

    if (base_data_matrix.empty() && error_message.empty()) {// If matrix is empty but no specific error was set
        error_message += "Data for feature '" + base_key + "' resulted in an empty matrix after fetching.\n";
    }
    if (!base_data_matrix.empty() && base_data_matrix.n_cols != timestamps.size()) {
        error_message += "Data for feature '" + base_key + "' has mismatched column count after fetching. Expected " + std::to_string(timestamps.size()) + " got " + std::to_string(base_data_matrix.n_cols) + ".\n";
        base_data_matrix.clear();// Invalidate matrix if dimensions are wrong
    }
    return base_data_matrix;
}

arma::Mat<double> TransformationBase::apply(
        DataManager * dm,
        std::string const & base_key,
        DM_DataType data_type,
        std::vector<std::size_t> const & timestamps,
        WhiskerTransformations::AppliedTransformation const & transform_config,
        std::string & error_message) const {
    if (!isSupported(data_type)) {
        error_message = "Data type '" + convert_data_type_to_string(data_type) + "' is not supported by this transformation for feature '" + base_key + ".";
        return arma::Mat<double>();// Return empty matrix
    }

    arma::Mat<double> base_data = fetchBaseData(dm, base_key, data_type, timestamps, error_message);
    if (!error_message.empty() || base_data.empty()) {
        // error_message would have been populated by fetchBaseData or the check above
        return arma::Mat<double>();// Return empty matrix
    }

    // Delegate to the specific transformation's logic
    return _applyTransformationLogic(base_data, transform_config, error_message);
}

bool TransformationBase::isSupported(DM_DataType type) const {
    return type == DM_DataType::Analog ||
           type == DM_DataType::DigitalInterval ||
           type == DM_DataType::Points ||
           type == DM_DataType::Tensor;
}
