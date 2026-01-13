#include "mlpack_conversion.hpp"


#include "DataManager/DigitalTimeSeries/Digital_Interval_Series.hpp"
#include "DataManager/Points/Point_Data.hpp"
#include "DataManager/Tensors/Tensor_Data.hpp"
#include "TimeFrame/TimeFrame.hpp"

#include <numeric>

/**
 * Convert a DigitalIntervalSeries to an mlpack row vector
 * @param series The DigitalIntervalSeries to convert
 * @param timestamps The timestamps to convert
 * @return arma::Row<double> The mlpack row vector
 */
arma::Row<double> convertToMlpackArray(
        DigitalIntervalSeries const * series,
        std::vector<std::size_t> timestamps) {

    auto length = timestamps.size();
    arma::Row<double> result(length, arma::fill::zeros);

    for (std::size_t i = 0; i < length; ++i) {
        if (series->hasIntervalAtTime(TimeFrameIndex(timestamps[i]), *(series->getTimeFrame()))) {
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
void updateDigitalIntervalSeriesFromMlpackArray(
        arma::Row<double> const & array,
        std::vector<std::size_t> & timestamps,
        DigitalIntervalSeries * series,
        float threshold) {
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
arma::Mat<double> convertToMlpackMatrix(
        PointData const * pointData,
        std::vector<std::size_t> const & timestamps) {

    size_t const numCols = timestamps.size();
    size_t const numRows = pointData->getMaxEntriesAtAnyTime() * 2;

    arma::Mat<double> result(numRows, numCols, arma::fill::zeros);

    auto col = 0;
    for (auto t: timestamps) {
        auto points = pointData->getAtTime(TimeFrameIndex(t));

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

void updatePointDataFromMlpackMatrix(
        arma::Mat<double> const & matrix,
        std::vector<std::size_t> & timestamps,
        PointData * pointData) {

    // Timestamp length should be same asmatrix n_cols * 2.

    if (timestamps.size() != matrix.n_cols * 2) {
        std::cerr << "Error: Timestamp length should be same as matrix n_cols * 2." << std::endl;
        return;
    }


    for (std::size_t col = 0; col < matrix.n_cols; ++col) {
        for (std::size_t row = 0; row < matrix.n_cols; row += 2) {
            if (matrix(row, col) != 0.0 || matrix(row + 1, col) != 0.0) {
                pointData->addAtTime(TimeFrameIndex(timestamps[col]),
                                     Point2D<float>{static_cast<float>(matrix(row, col)),
                                                    static_cast<float>(matrix(row + 1, col))},
                                     NotifyObservers::No);
            }
        }
    }
}

////////////////////////////////////////////////////////

// TensorData

/**
 * Convert a TensorData to an mlpack matrix
 * @param tensor_data The TensorData to convert
 * @param timestamps The timestamps to convert
 * @return arma::Mat<double> The mlpack matrix
 */
arma::Mat<double> convertTensorDataToMlpackMatrix(
        TensorData const * tensor_data,
        std::vector<std::size_t> const & timestamps) {
    // Determine the number of rows and columns for the Armadillo matrix
    std::size_t const numCols = timestamps.size();

    auto feature_shape = tensor_data->getFeatureShape();
    std::size_t const numRows = std::accumulate(
            feature_shape.begin(),
            feature_shape.end(),
            1,
            std::multiplies<>());

    // Initialize the Armadillo matrix
    arma::Mat<double> result(numRows, numCols, arma::fill::zeros);

    // Fill the matrix with the tensor data
    for (std::size_t col = 0; col < numCols; ++col) {
        // Use the backend-agnostic interface
        auto tensor_data_vec = tensor_data->getTensorDataAtTime(TimeFrameIndex(timestamps[col]));

        if (tensor_data_vec.empty()) {
            for (std::size_t row = 0; row < numRows; ++row) {
                result(row, col) = arma::datum::nan;
            }
            continue;
        }

        // tensor_data_vec is already flattened as a std::vector<float>
        for (std::size_t row = 0; row < tensor_data_vec.size() && row < numRows; ++row) {
            result(row, col) = static_cast<double>(tensor_data_vec[row]);
        }
    }

    return result;
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

    for (std::size_t i = 0; i < timestamps.size(); ++i) {
        auto col = copyMatrixRowToVector<double>(matrix.col(i));

        // Convert double vector to float vector for the backend-agnostic interface
        std::vector<float> float_data;
        float_data.reserve(col.size());
        for (double val: col) {
            float_data.push_back(static_cast<float>(val));
        }

        tensor_data.overwriteTensorAtTime(TimeFrameIndex(timestamps[i]), float_data, feature_shape);
    }
}

bool balance_training_data_by_subsampling(
        arma::Mat<double> const & features,
        arma::Row<size_t> const & labels,
        arma::Mat<double> & balanced_features,
        arma::Row<size_t> & balanced_labels,
        double max_ratio) {// Default max_ratio to 1.0

    std::cout << "Entering balancing training_data_by_subsampling function" << std::endl;

    if (features.n_cols != labels.n_elem || features.n_cols == 0) {
        std::cerr << "Error: Feature and label dimensions do not match or are empty." << std::endl;
        balanced_features = features;
        balanced_labels = labels;
        return false;
    }

    if (max_ratio < 1.0) {
        std::cerr << "Error: max_ratio must be >= 1.0" << std::endl;
        balanced_features = features;
        balanced_labels = labels;
        return false;
    }

    std::map<size_t, size_t> class_counts;
    for (size_t i = 0; i < labels.n_elem; ++i) {
        class_counts[labels[i]]++;
    }

    std::cout << "Class sizes: " << std::endl;
    for (auto const & [label_val, count]: class_counts) {
        std::cout << "Class " << label_val << " has " << count << " samples." << std::endl;
    }

    if (class_counts.empty()) {
        std::cerr << "Error: No classes found in labels." << std::endl;
        balanced_features = features;
        balanced_labels = labels;
        return false;
    }

    size_t min_class_count = std::numeric_limits<size_t>::max();
    size_t actual_max_class_count = 0;
    for (auto const & [label_val, count]: class_counts) {
        if (count < min_class_count) min_class_count = count;
        if (count > actual_max_class_count) actual_max_class_count = count;
    }

    if (min_class_count == 0) {
        std::cerr << "Warning: At least one class has zero samples in the original data. It will be ignored for balancing." << std::endl;
        // Try to find a non-zero min_class_count if other classes exist
        size_t temp_min_count = std::numeric_limits<size_t>::max();
        for (auto const & [label_val, count]: class_counts) {
            if (count > 0 && count < temp_min_count) temp_min_count = count;
        }
        if (temp_min_count == std::numeric_limits<size_t>::max()) {
            std::cerr << "Error: All classes effectively have zero samples for balancing." << std::endl;
            balanced_features = features;// Or empty, depending on desired behavior
            balanced_labels.clear();
            return false;
        }
        min_class_count = temp_min_count;
    }

    std::cout << "Minimum class count is " << min_class_count << std::endl;

    // If actual_max_class_count is significantly larger than min_class_count, print warning.
    // Using max_ratio in the condition to make it more relevant.
    if (actual_max_class_count > min_class_count * max_ratio && actual_max_class_count > min_class_count * 2.0) {// Heuristic for significant imbalance
        std::cout << "Warning: Training data is imbalanced. Original Max/Min ratio: "
                  << (min_class_count > 0 ? (double) actual_max_class_count / min_class_count : 0)
                  << ". Requested Max Ratio: " << max_ratio << std::endl;
    }

    // Find indices of each class in the labels vector
    std::vector<std::vector<size_t>> class_indices(class_counts.size());
    for (size_t i = 0; i < labels.n_elem; ++i) {
        class_indices[labels[i]].push_back(i);
    }

    //Shuffle each entry in class indices
    for (auto & indices: class_indices) {
        std::shuffle(indices.begin(), indices.end(), std::random_device());
    }

    // Subsample each class to have equal number of samples as the smallest class
    for (auto & indices: class_indices) {
        indices.resize(static_cast<size_t>(std::round(min_class_count * max_ratio)));
    }

    //Combine the indices
    std::vector<size_t> combined_indices;
    for (auto & indices: class_indices) {
        combined_indices.insert(combined_indices.end(), indices.begin(), indices.end());
    }

    std::cout << "Combined indices size: " << combined_indices.size() << std::endl;

    if (combined_indices.empty()) {
        std::cerr << "No data remains after attempting to balance. Check class counts and ratio." << std::endl;
        balanced_features.clear();
        balanced_labels.clear();
        return false;// Or true, if empty is an acceptable outcome of balancing
    }

    // Create balanced features and labels
    balanced_features.set_size(features.n_rows, combined_indices.size());
    balanced_labels.set_size(combined_indices.size());

    // Re-shuffle combined_indices to mix classes together before creating final matrix
    std::shuffle(combined_indices.begin(), combined_indices.end(), std::random_device());

    for (size_t i = 0; i < combined_indices.size(); ++i) {
        for (size_t j = 0; j < features.n_rows; ++j) {
            balanced_features(j, i) = features(j, combined_indices[i]);
        }
        balanced_labels[i] = labels[combined_indices[i]];
    }

    std::cout << "Balancing complete. Original samples: " << labels.n_elem
              << ", Balanced samples: " << balanced_labels.n_elem << std::endl;

    return true;
}
