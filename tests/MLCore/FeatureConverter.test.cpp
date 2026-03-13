
#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

#include "features/FeatureConverter.hpp"

#include "Tensors/TensorData.hpp"
#include "Tensors/RowDescriptor.hpp"
#include "TimeFrame/TimeFrame.hpp"
#include "TimeFrame/TimeIndexStorage.hpp"

#include <cmath>
#include <limits>
#include <memory>
#include <numeric>
#include <vector>

using Catch::Matchers::WithinAbs;

// ============================================================================
// Helpers
// ============================================================================

namespace {

std::shared_ptr<TimeFrame> makeTimeFrame(int count) {
    std::vector<int> times(count);
    for (int i = 0; i < count; ++i) {
        times[i] = i;
    }
    return std::make_shared<TimeFrame>(times);
}

std::shared_ptr<TimeIndexStorage> makeDenseTimeStorage(std::size_t count) {
    return TimeIndexStorageFactory::createDenseFromZero(count);
}

/**
 * @brief Create a known 2D ordinal tensor for testing
 *
 * Returns a 3×2 tensor:
 *   col0: [1, 4, 7]
 *   col1: [2, 5, 8]
 */
TensorData makeKnownTensor() {
    // Row-major: {1,2, 4,5, 7,8}
    std::vector<float> data = {1.0f, 2.0f, 4.0f, 5.0f, 7.0f, 8.0f};
    return TensorData::createOrdinal2D(data, 3, 2, {"x", "y"});
}

/**
 * @brief Create a tensor with NaN values in known positions
 *
 * 5×2 tensor, rows 1 and 3 have NaN in first column:
 *   row 0: [0, 1]
 *   row 1: [NaN, 3]
 *   row 2: [4, 5]
 *   row 3: [NaN, 7]
 *   row 4: [8, 9]
 */
TensorData makeTensorWithNaN() {
    std::vector<float> data = {
        0.0f, 1.0f,
        std::numeric_limits<float>::quiet_NaN(), 3.0f,
        4.0f, 5.0f,
        std::numeric_limits<float>::quiet_NaN(), 7.0f,
        8.0f, 9.0f
    };
    return TensorData::createOrdinal2D(data, 5, 2, {"a", "b"});
}

/**
 * @brief Create a tensor with Inf values
 */
TensorData makeTensorWithInf() {
    std::vector<float> data = {
        1.0f, 2.0f,
        3.0f, std::numeric_limits<float>::infinity(),
        5.0f, 6.0f
    };
    return TensorData::createOrdinal2D(data, 3, 2, {"a", "b"});
}

/**
 * @brief Create a time-indexed tensor for testing
 */
TensorData makeTimeTensor() {
    std::vector<float> data = {10.0f, 20.0f, 30.0f, 40.0f, 50.0f, 60.0f};
    auto ts = makeDenseTimeStorage(3);
    auto tf = makeTimeFrame(100);
    return TensorData::createTimeSeries2D(data, 3, 2, ts, tf, {"feat0", "feat1"});
}

} // anonymous namespace

// ============================================================================
// convertTensorToArma — basic conversion
// ============================================================================

TEST_CASE("FeatureConverter: basic conversion produces correct shape", "[FeatureConverter]") {
    auto tensor = makeKnownTensor();
    auto result = MLCore::convertTensorToArma(tensor);

    // mlpack layout: features × observations = 2 × 3
    CHECK(result.matrix.n_rows == 2);
    CHECK(result.matrix.n_cols == 3);
}

TEST_CASE("FeatureConverter: basic conversion preserves values", "[FeatureConverter]") {
    auto tensor = makeKnownTensor();
    auto result = MLCore::convertTensorToArma(tensor);

    // Original tensor (rows × cols):
    //   [1, 2]
    //   [4, 5]
    //   [7, 8]
    // Transposed (features × observations):
    //   row 0 (x): [1, 4, 7]
    //   row 1 (y): [2, 5, 8]

    CHECK_THAT(result.matrix(0, 0), WithinAbs(1.0, 1e-6));
    CHECK_THAT(result.matrix(0, 1), WithinAbs(4.0, 1e-6));
    CHECK_THAT(result.matrix(0, 2), WithinAbs(7.0, 1e-6));
    CHECK_THAT(result.matrix(1, 0), WithinAbs(2.0, 1e-6));
    CHECK_THAT(result.matrix(1, 1), WithinAbs(5.0, 1e-6));
    CHECK_THAT(result.matrix(1, 2), WithinAbs(8.0, 1e-6));
}

TEST_CASE("FeatureConverter: preserves column names", "[FeatureConverter]") {
    auto tensor = makeKnownTensor();
    auto result = MLCore::convertTensorToArma(tensor);

    REQUIRE(result.column_names.size() == 2);
    CHECK(result.column_names[0] == "x");
    CHECK(result.column_names[1] == "y");
}

TEST_CASE("FeatureConverter: generates feature names when not available", "[FeatureConverter]") {
    std::vector<float> data = {1.0f, 2.0f, 3.0f, 4.0f};
    auto tensor = TensorData::createOrdinal2D(data, 2, 2);

    auto result = MLCore::convertTensorToArma(tensor);

    REQUIRE(result.column_names.size() == 2);
    CHECK(result.column_names[0] == "feature_0");
    CHECK(result.column_names[1] == "feature_1");
}

TEST_CASE("FeatureConverter: valid row indices when no NaN dropping", "[FeatureConverter]") {
    auto tensor = makeKnownTensor();
    MLCore::ConversionConfig config;
    config.drop_nan = false;

    auto result = MLCore::convertTensorToArma(tensor, config);

    REQUIRE(result.valid_row_indices.size() == 3);
    CHECK(result.valid_row_indices[0] == 0);
    CHECK(result.valid_row_indices[1] == 1);
    CHECK(result.valid_row_indices[2] == 2);
    CHECK(result.rows_dropped == 0);
}

// ============================================================================
// convertTensorToArma — NaN/Inf handling
// ============================================================================

TEST_CASE("FeatureConverter: drops NaN rows by default", "[FeatureConverter]") {
    auto tensor = makeTensorWithNaN();
    auto result = MLCore::convertTensorToArma(tensor);

    // 5 original rows, 2 with NaN → 3 surviving
    CHECK(result.matrix.n_cols == 3);  // observations (surviving rows)
    CHECK(result.matrix.n_rows == 2);  // features
    CHECK(result.rows_dropped == 2);

    // Surviving rows: 0, 2, 4
    REQUIRE(result.valid_row_indices.size() == 3);
    CHECK(result.valid_row_indices[0] == 0);
    CHECK(result.valid_row_indices[1] == 2);
    CHECK(result.valid_row_indices[2] == 4);

    // Check surviving values (feature 0: [0, 4, 8], feature 1: [1, 5, 9])
    CHECK_THAT(result.matrix(0, 0), WithinAbs(0.0, 1e-6));
    CHECK_THAT(result.matrix(0, 1), WithinAbs(4.0, 1e-6));
    CHECK_THAT(result.matrix(0, 2), WithinAbs(8.0, 1e-6));
    CHECK_THAT(result.matrix(1, 0), WithinAbs(1.0, 1e-6));
    CHECK_THAT(result.matrix(1, 1), WithinAbs(5.0, 1e-6));
    CHECK_THAT(result.matrix(1, 2), WithinAbs(9.0, 1e-6));
}

TEST_CASE("FeatureConverter: drops Inf rows", "[FeatureConverter]") {
    auto tensor = makeTensorWithInf();
    auto result = MLCore::convertTensorToArma(tensor);

    // 3 original rows, 1 with Inf → 2 surviving
    CHECK(result.matrix.n_cols == 2);
    CHECK(result.rows_dropped == 1);

    REQUIRE(result.valid_row_indices.size() == 2);
    CHECK(result.valid_row_indices[0] == 0);
    CHECK(result.valid_row_indices[1] == 2);
}

TEST_CASE("FeatureConverter: keeps NaN rows when drop_nan is false", "[FeatureConverter]") {
    auto tensor = makeTensorWithNaN();
    MLCore::ConversionConfig config;
    config.drop_nan = false;

    auto result = MLCore::convertTensorToArma(tensor, config);

    CHECK(result.matrix.n_cols == 5);  // all rows kept
    CHECK(result.rows_dropped == 0);
    CHECK(result.valid_row_indices.size() == 5);

    // NaN values are preserved (mlpack may handle or crash — user's choice)
    CHECK(std::isnan(result.matrix(0, 1)));  // feature 0, observation 1 = NaN
    CHECK(std::isnan(result.matrix(0, 3)));  // feature 0, observation 3 = NaN
}

TEST_CASE("FeatureConverter: clean tensor has no rows dropped", "[FeatureConverter]") {
    auto tensor = makeKnownTensor();
    auto result = MLCore::convertTensorToArma(tensor);

    CHECK(result.rows_dropped == 0);
    CHECK(result.valid_row_indices.size() == 3);
}

// ============================================================================
// convertTensorToArma — z-score normalization
// ============================================================================

TEST_CASE("FeatureConverter: z-score normalization produces zero-mean unit-variance", "[FeatureConverter]") {
    // Create a tensor with a known distribution
    std::vector<float> data;
    std::size_t const rows = 100;
    std::size_t const cols = 2;
    data.reserve(rows * cols);

    // Column 0: values 0..99, Column 1: values 100..199
    for (std::size_t r = 0; r < rows; ++r) {
        data.push_back(static_cast<float>(r));
        data.push_back(static_cast<float>(r + 100));
    }
    auto tensor = TensorData::createOrdinal2D(data, rows, cols, {"low", "high"});

    MLCore::ConversionConfig config;
    config.zscore_normalize = true;

    auto result = MLCore::convertTensorToArma(tensor, config);

    // features × observations = 2 × 100
    REQUIRE(result.matrix.n_rows == 2);
    REQUIRE(result.matrix.n_cols == 100);

    // Each feature row should have mean ≈ 0 and std ≈ 1
    for (arma::uword f = 0; f < 2; ++f) {
        arma::rowvec row = result.matrix.row(f);
        double mean = arma::mean(row);
        double sd = arma::stddev(row, 0);

        CHECK_THAT(mean, WithinAbs(0.0, 1e-6));
        CHECK_THAT(sd, WithinAbs(1.0, 0.02));  // slight tolerance due to epsilon
    }

    // Z-score parameters should be populated
    REQUIRE(result.zscore_means.size() == 2);
    REQUIRE(result.zscore_stds.size() == 2);
    CHECK_THAT(result.zscore_means[0], WithinAbs(49.5, 0.1));    // mean of 0..99
    CHECK_THAT(result.zscore_means[1], WithinAbs(149.5, 0.1));   // mean of 100..199
}

TEST_CASE("FeatureConverter: no z-score params when not normalizing", "[FeatureConverter]") {
    auto tensor = makeKnownTensor();
    MLCore::ConversionConfig config;
    config.zscore_normalize = false;

    auto result = MLCore::convertTensorToArma(tensor, config);

    CHECK(result.zscore_means.empty());
    CHECK(result.zscore_stds.empty());
}

TEST_CASE("FeatureConverter: z-score with constant column uses epsilon", "[FeatureConverter]") {
    // Column where all values are the same (std = 0)
    std::vector<float> data = {5.0f, 3.0f, 5.0f, 6.0f, 5.0f, 9.0f};
    auto tensor = TensorData::createOrdinal2D(data, 3, 2, {"const_col", "varying_col"});

    MLCore::ConversionConfig config;
    config.zscore_normalize = true;

    // Should not crash even though column 0 may have varied std
    auto result = MLCore::convertTensorToArma(tensor, config);

    // Matrix should exist and be finite
    CHECK(result.matrix.is_finite());
}

// ============================================================================
// convertTensorToArmaRowMajor
// ============================================================================

TEST_CASE("FeatureConverter: row-major conversion preserves observations x features layout", "[FeatureConverter]") {
    auto tensor = makeKnownTensor();
    auto result = MLCore::convertTensorToArmaRowMajor(tensor);

    // observations × features = 3 × 2
    CHECK(result.matrix.n_rows == 3);
    CHECK(result.matrix.n_cols == 2);

    // Original values preserved
    CHECK_THAT(result.matrix(0, 0), WithinAbs(1.0, 1e-6));
    CHECK_THAT(result.matrix(0, 1), WithinAbs(2.0, 1e-6));
    CHECK_THAT(result.matrix(1, 0), WithinAbs(4.0, 1e-6));
    CHECK_THAT(result.matrix(1, 1), WithinAbs(5.0, 1e-6));
    CHECK_THAT(result.matrix(2, 0), WithinAbs(7.0, 1e-6));
    CHECK_THAT(result.matrix(2, 1), WithinAbs(8.0, 1e-6));
}

TEST_CASE("FeatureConverter: row-major drops NaN rows too", "[FeatureConverter]") {
    auto tensor = makeTensorWithNaN();
    auto result = MLCore::convertTensorToArmaRowMajor(tensor);

    CHECK(result.matrix.n_rows == 3);  // 5 - 2 NaN rows
    CHECK(result.matrix.n_cols == 2);
    CHECK(result.rows_dropped == 2);
}

TEST_CASE("FeatureConverter: row-major z-score normalizes columns", "[FeatureConverter]") {
    std::vector<float> data;
    std::size_t const rows = 50;
    std::size_t const cols = 2;
    for (std::size_t r = 0; r < rows; ++r) {
        data.push_back(static_cast<float>(r));
        data.push_back(static_cast<float>(r * 2));
    }
    auto tensor = TensorData::createOrdinal2D(data, rows, cols);

    MLCore::ConversionConfig config;
    config.zscore_normalize = true;
    auto result = MLCore::convertTensorToArmaRowMajor(tensor, config);

    // Each column should have mean ≈ 0
    for (arma::uword c = 0; c < 2; ++c) {
        double mean = arma::mean(result.matrix.col(c));
        CHECK_THAT(mean, WithinAbs(0.0, 1e-6));
    }
}

// ============================================================================
// convertTensorToArma — time-indexed tensors
// ============================================================================

TEST_CASE("FeatureConverter: works with time-indexed tensor", "[FeatureConverter]") {
    auto tensor = makeTimeTensor();
    auto result = MLCore::convertTensorToArma(tensor);

    // features × observations = 2 × 3
    CHECK(result.matrix.n_rows == 2);
    CHECK(result.matrix.n_cols == 3);

    REQUIRE(result.column_names.size() == 2);
    CHECK(result.column_names[0] == "feat0");
    CHECK(result.column_names[1] == "feat1");
}

// ============================================================================
// convertTensorToArma — error cases
// ============================================================================

TEST_CASE("FeatureConverter: throws on empty tensor", "[FeatureConverter]") {
    TensorData empty;
    CHECK_THROWS_AS(MLCore::convertTensorToArma(empty), std::invalid_argument);
}

TEST_CASE("FeatureConverter: throws on 1D tensor", "[FeatureConverter]") {
    std::vector<float> data = {1.0f, 2.0f, 3.0f};
    std::vector<AxisDescriptor> axes = {{"values", 3}};
    auto tensor = TensorData::createND(data, axes);

    CHECK_THROWS_AS(MLCore::convertTensorToArma(tensor), std::invalid_argument);
}

TEST_CASE("FeatureConverter: throws on 3D tensor", "[FeatureConverter]") {
    std::vector<float> data(2 * 3 * 4, 1.0f);
    std::vector<AxisDescriptor> axes = {{"batch", 2}, {"rows", 3}, {"cols", 4}};
    auto tensor = TensorData::createND(data, axes);

    CHECK_THROWS_AS(MLCore::convertTensorToArma(tensor), std::invalid_argument);
}

// ============================================================================
// applyZscoreNormalization
// ============================================================================

TEST_CASE("FeatureConverter: applyZscoreNormalization applies saved parameters", "[FeatureConverter]") {
    // First, normalize a dataset and capture parameters
    std::vector<float> train_data;
    for (int i = 0; i < 100; ++i) {
        train_data.push_back(static_cast<float>(i));
        train_data.push_back(static_cast<float>(i * 2 + 50));
    }
    auto train_tensor = TensorData::createOrdinal2D(train_data, 100, 2);

    MLCore::ConversionConfig config;
    config.zscore_normalize = true;
    auto train_result = MLCore::convertTensorToArma(train_tensor, config);

    // Now apply same parameters to new data
    std::vector<float> test_data = {50.0f, 150.0f, 25.0f, 100.0f};
    auto test_tensor = TensorData::createOrdinal2D(test_data, 2, 2);

    MLCore::ConversionConfig no_norm;
    no_norm.zscore_normalize = false;
    auto test_result = MLCore::convertTensorToArma(test_tensor, no_norm);

    // Apply training normalization parameters
    MLCore::applyZscoreNormalization(
        test_result.matrix,
        train_result.zscore_means,
        train_result.zscore_stds);

    // Matrix should be finite
    CHECK(test_result.matrix.is_finite());

    // First observation (50, 150) should be close to zero after normalization
    // since training mean was ~49.5 for feature 0
    CHECK_THAT(test_result.matrix(0, 0), WithinAbs(0.0, 0.1));
}

TEST_CASE("FeatureConverter: applyZscoreNormalization throws on size mismatch", "[FeatureConverter]") {
    arma::mat mat(3, 5, arma::fill::ones);  // 3 features × 5 observations
    std::vector<double> wrong_means = {0.0, 0.0};  // only 2, need 3
    std::vector<double> stds = {1.0, 1.0};

    CHECK_THROWS_AS(
        MLCore::applyZscoreNormalization(mat, wrong_means, stds),
        std::invalid_argument);
}

// ============================================================================
// zscoreNormalize standalone function
// ============================================================================

TEST_CASE("FeatureConverter: zscoreNormalize returns correct parameters", "[FeatureConverter]") {
    // Create a features × observations matrix (2 features, 4 observations)
    arma::mat mat = {{1.0, 2.0, 3.0, 4.0},
                     {10.0, 20.0, 30.0, 40.0}};

    auto [means, stds] = MLCore::zscoreNormalize(mat);

    REQUIRE(means.size() == 2);
    REQUIRE(stds.size() == 2);

    CHECK_THAT(means[0], WithinAbs(2.5, 1e-6));    // mean of [1,2,3,4]
    CHECK_THAT(means[1], WithinAbs(25.0, 1e-6));   // mean of [10,20,30,40]

    // After normalization, mean should be ≈ 0
    CHECK_THAT(arma::mean(mat.row(0)), WithinAbs(0.0, 1e-6));
    CHECK_THAT(arma::mean(mat.row(1)), WithinAbs(0.0, 1e-6));
}

// ============================================================================
// Edge cases
// ============================================================================

TEST_CASE("FeatureConverter: single row tensor converts correctly", "[FeatureConverter]") {
    std::vector<float> data = {1.0f, 2.0f, 3.0f};
    auto tensor = TensorData::createOrdinal2D(data, 1, 3);

    auto result = MLCore::convertTensorToArma(tensor);

    CHECK(result.matrix.n_rows == 3);  // 3 features
    CHECK(result.matrix.n_cols == 1);  // 1 observation
    CHECK(result.rows_dropped == 0);
}

TEST_CASE("FeatureConverter: single column tensor converts correctly", "[FeatureConverter]") {
    std::vector<float> data = {1.0f, 2.0f, 3.0f};
    auto tensor = TensorData::createOrdinal2D(data, 3, 1);

    auto result = MLCore::convertTensorToArma(tensor);

    CHECK(result.matrix.n_rows == 1);  // 1 feature
    CHECK(result.matrix.n_cols == 3);  // 3 observations
}

TEST_CASE("FeatureConverter: all rows NaN results in empty matrix", "[FeatureConverter]") {
    float nan = std::numeric_limits<float>::quiet_NaN();
    std::vector<float> data = {nan, 1.0f, nan, 2.0f, nan, 3.0f};
    auto tensor = TensorData::createOrdinal2D(data, 3, 2);

    auto result = MLCore::convertTensorToArma(tensor);

    CHECK(result.matrix.n_cols == 0);  // all rows dropped
    CHECK(result.rows_dropped == 3);
    CHECK(result.valid_row_indices.empty());
}
