#ifndef WHISKERTOOLBOX_MLPACK_CONVERSION_HPP
#define WHISKERTOOLBOX_MLPACK_CONVERSION_HPP


#include <armadillo>

#include <memory>

class DigitalIntervalSeries;
class PointData;
class TensorData;

////////////////////////////////////////////////////////

// DigitalIntervalSeries

/**
 * Convert a DigitalIntervalSeries to an mlpack row vector
 * @param series The DigitalIntervalSeries to convert
 * @param timestamps The timestamps to convert
 * @return arma::Row<double> The mlpack row vector
 */
arma::Row<double> convertToMlpackArray(
        std::shared_ptr<DigitalIntervalSeries> const & series,
        std::vector<std::size_t> timestamps);

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
        float threshold = 0.5);

////////////////////////////////////////////////////////

// PointData

/**
 * Convert a PointData to an mlpack matrix
 * @param pointData The PointData to convert
 * @param timestamps The timestamps to convert
 * @return arma::Mat<double> The mlpack matrix
 */
arma::Mat<double> convertToMlpackMatrix(
        std::shared_ptr<PointData> const & pointData,
        std::vector<std::size_t> const & timestamps);

void updatePointDataFromMlpackMatrix(
        arma::Mat<double> const & matrix,
        std::vector<std::size_t> & timestamps,
        PointData * pointData);

////////////////////////////////////////////////////////

// TensorData

/**
 * Convert a TensorData to an mlpack matrix
 * @param tensor_data The TensorData to convert
 * @param timestamps The timestamps to convert
 * @return arma::Mat<double> The mlpack matrix
 */
arma::Mat<double> convertTensorDataToMlpackMatrix(
        TensorData const & tensor_data,
        std::vector<std::size_t> const & timestamps);

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
void updateTensorDataFromMlpackMatrix(
        arma::Mat<double> const & matrix,
        std::vector<std::size_t> const & timestamps,
        TensorData & tensor_data);

/**
 * @brief Balances training data by subsampling features and labels based on a max ratio.
 *
 * Calculates class counts. Subsamples such that for any class,
 * its sample count is at most `min_class_count * max_ratio`.
 * Ensures that each class still present after potential subsampling due to `max_ratio`
 * will have `std::min(original_sample_count_for_this_class, target_samples_for_this_class)` samples.
 *
 * @param features Input feature matrix (arma::Mat<double>).
 * @param labels Input label row vector (arma::Row<size_t>).
 * @param balanced_features Output balanced feature matrix.
 * @param balanced_labels Output balanced label row vector.
 * @param max_ratio Maximum ratio of any class to the smallest class (e.g., 1.0 means 1:1, 2.0 means 2:1 for majority vs minority).
 * @return True if balancing was performed, false if an error occurred.
 */
bool balance_training_data_by_subsampling(
        arma::Mat<double> const& features,
        arma::Row<size_t> const& labels,
        arma::Mat<double>& balanced_features,
        arma::Row<size_t>& balanced_labels,
        double max_ratio = 1.0);

#endif//WHISKERTOOLBOX_MLPACK_CONVERSION_HPP
