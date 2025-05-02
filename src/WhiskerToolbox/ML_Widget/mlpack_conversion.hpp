
#ifndef WHISKERTOOLBOX_MLPACK_CONVERSION_HPP
#define WHISKERTOOLBOX_MLPACK_CONVERSION_HPP

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
        std::shared_ptr<DigitalIntervalSeries> const & series,
        std::vector<std::size_t> timestamps) {

    auto length = timestamps.size();
    arma::Row<double> result(length, arma::fill::zeros);

    for (std::size_t i = 0; i < length; ++i) {
        if (series->isEventAtTime(static_cast<int>(timestamps[i]))) {
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
        arma::Row<double> const & array,
        std::vector<std::size_t> & timestamps,
        DigitalIntervalSeries * series,
        float threshold = 0.5) {
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
        std::shared_ptr<PointData> const & pointData,
        std::vector<std::size_t> & timestamps) {

    size_t const numCols = timestamps.size();
    size_t const numRows = pointData->getMaxPoints() * 2;

    arma::Mat<double> result(numRows, numCols, arma::fill::zeros);

    auto col = 0;
    for (auto t: timestamps) {
        auto points = pointData->getPointsAtTime(static_cast<int>(t));

        if (points.empty()) {
            for (std::size_t i = 0; i < numRows; ++i) {
                result(i, col) = arma::datum::nan;
            }
            col++;
            continue;
        }

        size_t const row = 0;
        for (auto p: points) {
            result(row * 2, col) = p.x;
            result(row * 2 + 1, col) = p.y;
        }
        col++;
    }

    return result;
}

inline void updatePointDataFromMlpackMatrix(
        arma::Mat<double> const & matrix,
        std::vector<std::size_t> & timestamps,
        PointData * pointData) {

    std::vector<std::vector<Point2D<float>>> points;

    for (std::size_t col = 0; col < matrix.n_cols; ++col) {
        points.emplace_back();
        for (std::size_t row = 0; row < matrix.n_cols; row += 2) {
            if (matrix(row, col) != 0.0 || matrix(row + 1, col) != 0.0) {
                points.back().emplace_back(Point2D<float>{
                        static_cast<float>(matrix(row, col)),
                        static_cast<float>(matrix(row + 1, col))});
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
        TensorData const & tensor_data,
        std::vector<std::size_t> const & timestamps) {
    // Determine the number of rows and columns for the Armadillo matrix
    std::size_t const numCols = timestamps.size();

    auto feature_shape = tensor_data.getFeatureShape();
    std::size_t const numRows = std::accumulate(
            feature_shape.begin(),
            feature_shape.end(),
            1,
            std::multiplies<>());

    // Initialize the Armadillo matrix
    arma::Mat<double> result(numRows, numCols, arma::fill::zeros);

    // Fill the matrix with the tensor data
    for (std::size_t col = 0; col < numCols; ++col) {
        auto tensor = tensor_data.getTensorAtTime(static_cast<int>(timestamps[col]));

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

template<typename T>
std::vector<T> copyMatrixRowToVector(arma::Row<T> const & row) {
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
        arma::Mat<double> const & matrix,
        std::vector<std::size_t> const & timestamps,
        TensorData & tensor_data) {
    auto feature_shape = tensor_data.getFeatureShape();
    std::vector<int64_t> const shape(feature_shape.begin(), feature_shape.end());

    for (std::size_t i = 0; i < timestamps.size(); ++i) {
        auto col = copyMatrixRowToVector<double>(matrix.col(i));
        torch::Tensor const tensor = torch::from_blob(col.data(), shape, torch::kDouble).clone();
        tensor_data.overwriteTensorAtTime(static_cast<int>(timestamps[i]), tensor);
    }
}

#endif//WHISKERTOOLBOX_MLPACK_CONVERSION_HPP
