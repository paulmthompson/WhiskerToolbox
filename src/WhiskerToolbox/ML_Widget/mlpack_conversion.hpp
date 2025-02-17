
#ifndef WHISKERTOOLBOX_MLPACK_CONVERSION_HPP
#define WHISKERTOOLBOX_MLPACK_CONVERSION_HPP

#include "DataManager/AnalogTimeSeries/Analog_Time_Series.hpp"
#include "DataManager/DigitalTimeSeries/Digital_Interval_Series.hpp"
#include "DataManager/Points/Point_Data.hpp"
#include "DataManager/Tensors/Tensor_Data.hpp"

#include <mlpack/core.hpp>

#include <memory>

////////////////////////////////////////////////////////

// DigitalIntervalSeries

/**
 * Convert a DigitalIntervalSeries to an mlpack row vector
 * @param series The DigitalIntervalSeries to convert
 * @param timestamps The timestamps to convert
 * @return arma::Row<double> The mlpack row vector
 */
inline arma::Row<double> convertToMlpackArray(
        const std::shared_ptr<DigitalIntervalSeries>& series,
        std::vector<std::size_t> timestamps)
{

    auto length = timestamps.size();
    arma::Row<double> result(length, arma::fill::zeros);

    for (std::size_t i = 0; i < length; ++i) {
        if (series->isEventAtTime(timestamps[i])) {
            result[i] = 1.0;
        }
    }

    return result;
}

/**
 * Update a DigitalIntervalSeries from an mlpack row vector
 * @param array The mlpack row vector
 * @param timestamps The timestamps to update
 * @param series The DigitalIntervalSeries to update
 * @param threshold The threshold to use for the update
 */
inline void updateDigitalIntervalSeriesFromMlpackArray(
        const arma::Row<double>& array,
        std::vector<std::size_t>& timestamps,
        std::shared_ptr<DigitalIntervalSeries> series,
        float threshold = 0.5)
{
    //convert array to vector of bool based on some threshold

    std::vector<bool> events;
    for (std::size_t i = 0; i < array.n_elem; ++i) {
        events.push_back(array[i] > threshold);
    }

    series->setEventsAtTimes(timestamps, events);
}

////////////////////////////////////////////////////////

// PointData

/**
 * Convert a PointData to an mlpack matrix
 * @param pointData The PointData to convert
 * @param timestamps The timestamps to convert
 * @return arma::Mat<double> The mlpack matrix
 */
inline arma::Mat<double> convertToMlpackMatrix(
        const std::shared_ptr<PointData>& pointData,
        std::vector<std::size_t>& timestamps)
{

    const size_t numCols = timestamps.size();
    const size_t numRows = pointData->getMaxPoints() * 2;

    arma::Mat<double> result(numRows, numCols, arma::fill::zeros);

    auto col = 0;
    for (auto t : timestamps)
    {
        auto points = pointData->getPointsAtTime(t);

        if (points.empty()) {
            for (std::size_t i = 0; i < numRows; ++i) {
                result(i, col) = arma::datum::nan;
            }
            col++;
            continue;
        }

        auto row = 0;
        for (auto p : points) {
            result(row * 2, col) = p.x;
            result(row * 2 + 1, col) = p.y;
        }
        col++;
    }

    return result;
}

inline void updatePointDataFromMlpackMatrix(
        const arma::Mat<double>& matrix,
        std::vector<std::size_t>& timestamps,
        std::shared_ptr<PointData> pointData)
{

    std::vector<std::vector<Point2D<float>>> points;

    for (std::size_t col = 0; col < matrix.n_cols; ++col) {
        points.push_back(std::vector<Point2D<float>>());
        for (std::size_t row = 0; row < matrix.n_cols; row += 2) {
            if (matrix(row, col) != 0.0 || matrix(row + 1, col) != 0.0) {
                points.back().emplace_back(Point2D<float>{
                        static_cast<float>(matrix(row, col)),
                        static_cast<float>(matrix(row + 1, col))
                });
            }
        }
    }
    pointData->overwritePointsAtTimes(timestamps, points);
}

////////////////////////////////////////////////////////

// TensorData

/**
 * Convert a TensorData to an mlpack matrix
 * @param tensor_data The TensorData to convert
 * @param timestamps The timestamps to convert
 * @return arma::Mat<double> The mlpack matrix
 */
inline arma::Mat<double> convertTensorDataToMlpackMatrix(
        const TensorData& tensor_data,
        const std::vector<std::size_t>& timestamps)
{
    // Determine the number of rows and columns for the Armadillo matrix
    std::size_t numCols = timestamps.size();

    auto feature_shape = tensor_data.getFeatureShape();
    std::size_t numRows = std::accumulate(
            feature_shape.begin(),
            feature_shape.end(),
            1,
            std::multiplies<std::size_t>());

    // Initialize the Armadillo matrix
    arma::Mat<double> result(numRows, numCols, arma::fill::zeros);

    // Fill the matrix with the tensor data
    for (std::size_t col = 0; col < numCols; ++col) {
        auto tensor = tensor_data.getTensorAtTime(timestamps[col]);

        if (tensor.numel() == 0) {
            for (std::size_t row = 0; row < numRows; ++row) {
                result(row, col) = arma::datum::nan;
            }
            continue;
        }

        auto flattened_tensor = tensor.flatten();
        auto tensor_data_ptr = flattened_tensor.data_ptr<float>();

        for (std::size_t row = 0; row < flattened_tensor.numel(); ++row) {
            result(row, col) = static_cast<double>(tensor_data_ptr[row]);
        }
    }

    return result;
}

template <typename T>
std::vector<T> copyMatrixRowToVector(const arma::Row<T>& row) {
    std::vector<T> vec(row.n_elem);
    for (std::size_t i = 0; i < row.n_elem; ++i) {
        vec[i] = row[i];
    }
    return vec;
}

/**
 * Update a TensorData from an mlpack matrix
 * @param matrix The mlpack matrix
 * @param timestamps The timestamps to update
 * @param tensor_data The TensorData to update
 */
inline void updateTensorDataFromMlpackMatrix(
        const arma::Mat<double>& matrix,
        const std::vector<std::size_t>& timestamps,
        TensorData& tensor_data)
{
    auto feature_shape = tensor_data.getFeatureShape();
    std::vector<int64_t> shape(feature_shape.begin(), feature_shape.end());

    for (std::size_t i = 0; i < timestamps.size(); ++i) {
        auto col = copyMatrixRowToVector<double>(matrix.col(i));
        torch::Tensor tensor = torch::from_blob(col.data(), shape, torch::kDouble).clone();
        tensor_data.overwriteTensorAtTime(timestamps[i], tensor);
    }
}

////////////////////////////////////////////////////////

//AnalogTimeSeries

/**
 * Convert an AnalogTimeSeries to an mlpack row vector
 * @param analogTimeSeries The AnalogTimeSeries to convert
 * @param timestamps The timestamps to convert
 * @return arma::Row<double> The mlpack row vector
 */
inline arma::Row<double> convertAnalogTimeSeriesToMlpackArray(
        const std::shared_ptr<AnalogTimeSeries>& analogTimeSeries,
        std::vector<std::size_t>& timestamps)
{
    auto length = timestamps.size();
    arma::Row<double> result(length, arma::fill::zeros);

    const auto& data = analogTimeSeries->getAnalogTimeSeries();
    const auto& time = analogTimeSeries->getTimeSeries();

    for (std::size_t i = 0; i < length; ++i) {
        auto it = std::find(time.begin(), time.end(), timestamps[i]);
        if (it != time.end()) {
            result[i] = data[std::distance(time.begin(), it)];
        } else {
            result[i] = arma::datum::nan;
        }
    }

    return result;
}

/**
 * Update an AnalogTimeSeries from an mlpack row vector
 * @param array The mlpack row vector
 * @param timestamps The timestamps to update
 * @param analogTimeSeries The AnalogTimeSeries to update
 */
inline void updateAnalogTimeSeriesFromMlpackArray(
        const arma::Row<double>& array,
        std::vector<std::size_t>& timestamps,
        std::shared_ptr<AnalogTimeSeries> analogTimeSeries)
{
    std::vector<float> data(array.n_elem);

    analogTimeSeries->overwriteAtTimes(data, timestamps);
}

#endif //WHISKERTOOLBOX_MLPACK_CONVERSION_HPP
