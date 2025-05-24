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

/**
 * @brief Balances training data by subsampling features and labels.
 *
 * Calculates class counts from the input labels. If the data is imbalanced
 * (max_class_count > 2 * min_class_count), it prints a warning.
 * Subsamples the features and labels so that each class has a number of samples
 * equal to the count of the smallest represented class (min_class_count).
 *
 * @param features Input feature matrix (arma::Mat<double>).
 * @param labels Input label row vector (arma::Row<size_t>).
 * @param balanced_features Output balanced feature matrix.
 * @param balanced_labels Output balanced label row vector.
 * @return True if balancing was performed (even if no subsampling was needed), false if an error occurred (e.g., empty inputs).
 */
inline bool balance_training_data_by_subsampling(
    arma::Mat<double> const& features,
    arma::Row<size_t> const& labels,
    arma::Mat<double>& balanced_features,
    arma::Row<size_t>& balanced_labels) {

    if (features.n_cols != labels.n_elem || features.n_cols == 0) {
        std::cerr << "Error: Feature and label dimensions do not match or are empty." << std::endl;
        balanced_features = features; // Pass through original data on error
        balanced_labels = labels;
        return false;
    }

    std::map<size_t, size_t> class_counts;
    for (size_t i = 0; i < labels.n_elem; ++i) {
        class_counts[labels[i]]++;
    }

    if (class_counts.empty()) {
        std::cerr << "Error: No classes found in labels." << std::endl;
        balanced_features = features;
        balanced_labels = labels;
        return false;
    }

    size_t min_class_count = std::numeric_limits<size_t>::max();
    size_t max_class_count = 0;
    for (auto const& [label_val, count] : class_counts) {
        if (count < min_class_count) min_class_count = count;
        if (count > max_class_count) max_class_count = count;
    }

    if (min_class_count == 0 && !class_counts.empty()) { // Should not happen if labels.n_elem > 0
        std::cerr << "Warning: At least one class has zero samples after counting. Check labels." << std::endl;
        // Decide how to handle this: either error out or try to proceed with available classes.
        // For now, let's try to find a non-zero min_class_count if possible or error.
        min_class_count = std::numeric_limits<size_t>::max();
        for (auto const& [label_val, count] : class_counts) {
            if (count > 0 && count < min_class_count) min_class_count = count;
        }
        if (min_class_count == std::numeric_limits<size_t>::max()) {
             std::cerr << "Error: All classes have zero samples or an issue with label counting." << std::endl;
             balanced_features = features;
             balanced_labels = labels;
             return false;
        }
    }

    if (max_class_count > 2 * min_class_count && min_class_count > 0) {
        std::cout << "Warning: Training data is imbalanced. Max class count (" << max_class_count
                  << ") is more than double the min class count (" << min_class_count << ")." << std::endl;
    }
    
    if (min_class_count == 0) { // If after all checks min_class_count is still 0, cannot balance.
        std::cerr << "Error: Cannot balance data as minimum class count is zero." << std::endl;
        balanced_features = features;
        balanced_labels = labels;
        return false;
    }

    // Find indices of each class in the labels vector
    std::vector<std::vector<size_t>> class_indices(class_counts.size());
    for (size_t i = 0; i < labels.n_elem; ++i) {
        class_indices[labels[i]].push_back(i);
    }

    //Shuffle each entry in class indices
    for (auto& indices : class_indices) {
        std::shuffle(indices.begin(), indices.end(), std::random_device());
    }

    // Subsample each class to have equal number of samples as the smallest class
    for (auto& indices : class_indices) {
        indices.resize(min_class_count);
    }

    //Combine the indices
    std::vector<size_t> combined_indices;
    for (auto& indices : class_indices) {
        combined_indices.insert(combined_indices.end(), indices.begin(), indices.end());
    }

    std::cout << "Combined indices size: " << combined_indices.size() << std::endl;

    // Create balanced features and labels
    balanced_features.set_size(features.n_rows, combined_indices.size());
    balanced_labels.set_size(combined_indices.size());

    for (size_t i = 0; i < combined_indices.size(); ++i) {
        for (size_t j = 0; j < features.n_rows; ++j) {
            balanced_features(j, i) = features(j, combined_indices[i]);
        }
        balanced_labels[i] = labels[combined_indices[i]];
    }

    return true;
}

#endif//WHISKERTOOLBOX_MLPACK_CONVERSION_HPP
