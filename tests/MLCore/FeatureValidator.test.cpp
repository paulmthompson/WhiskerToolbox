
#include <catch2/catch_test_macros.hpp>

#include "features/FeatureValidator.hpp"

#include "DataManager/Tensors/TensorData.hpp"
#include "DataManager/Tensors/RowDescriptor.hpp"
#include "TimeFrame/TimeFrame.hpp"
#include "TimeFrame/TimeIndexStorage.hpp"

#include <cmath>
#include <limits>
#include <memory>
#include <vector>

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
 * @brief Create a simple 2D tensor with ordinal rows
 */
TensorData makeOrdinalTensor(std::size_t rows, std::size_t cols) {
    std::vector<float> data(rows * cols, 1.0f);
    for (std::size_t i = 0; i < data.size(); ++i) {
        data[i] = static_cast<float>(i);
    }
    std::vector<std::string> names;
    for (std::size_t c = 0; c < cols; ++c) {
        names.push_back("col_" + std::to_string(c));
    }
    return TensorData::createOrdinal2D(data, rows, cols, names);
}

/**
 * @brief Create a 2D tensor with TimeFrameIndex rows
 */
TensorData makeTimeTensor(std::size_t rows, std::size_t cols) {
    std::vector<float> data(rows * cols, 1.0f);
    for (std::size_t i = 0; i < data.size(); ++i) {
        data[i] = static_cast<float>(i);
    }
    auto ts = makeDenseTimeStorage(rows);
    auto tf = makeTimeFrame(static_cast<int>(rows + 10));
    std::vector<std::string> names;
    for (std::size_t c = 0; c < cols; ++c) {
        names.push_back("feat_" + std::to_string(c));
    }
    return TensorData::createTimeSeries2D(data, rows, cols, ts, tf, names);
}

/**
 * @brief Create a 2D tensor with Interval rows
 */
TensorData makeIntervalTensor(std::size_t rows, std::size_t cols) {
    std::vector<float> data(rows * cols, 1.0f);
    for (std::size_t i = 0; i < data.size(); ++i) {
        data[i] = static_cast<float>(i);
    }
    std::vector<TimeFrameInterval> intervals;
    for (std::size_t i = 0; i < rows; ++i) {
        auto start = TimeFrameIndex(static_cast<int64_t>(i * 10));
        auto end = TimeFrameIndex(static_cast<int64_t>(i * 10 + 9));
        intervals.push_back({start, end});
    }
    auto tf = makeTimeFrame(static_cast<int>(rows * 10 + 20));
    std::vector<std::string> names;
    for (std::size_t c = 0; c < cols; ++c) {
        names.push_back("feat_" + std::to_string(c));
    }
    return TensorData::createFromIntervals(data, rows, cols, intervals, tf, names);
}

/**
 * @brief Create a tensor with some NaN values
 */
TensorData makeTensorWithNaNs(std::size_t rows, std::size_t cols,
                               std::vector<std::size_t> const & nan_rows) {
    std::vector<float> data(rows * cols, 1.0f);
    for (std::size_t i = 0; i < data.size(); ++i) {
        data[i] = static_cast<float>(i);
    }
    // Set first column of specified rows to NaN
    for (auto r : nan_rows) {
        data[r * cols] = std::numeric_limits<float>::quiet_NaN();
    }
    return TensorData::createOrdinal2D(data, rows, cols);
}

} // anonymous namespace

// ============================================================================
// validateFeatureTensor — standalone tensor validation
// ============================================================================

TEST_CASE("FeatureValidator: empty tensor fails validation", "[FeatureValidator]") {
    TensorData empty;
    auto result = MLCore::validateFeatureTensor(empty);

    CHECK_FALSE(result.valid);
    CHECK(result.reason == MLCore::RowCompatibility::EmptyTensor);
}

TEST_CASE("FeatureValidator: valid 2D ordinal tensor passes", "[FeatureValidator]") {
    auto tensor = makeOrdinalTensor(10, 3);
    auto result = MLCore::validateFeatureTensor(tensor);

    CHECK(result.valid);
    CHECK(result.reason == MLCore::RowCompatibility::Compatible);
}

TEST_CASE("FeatureValidator: valid 2D time-indexed tensor passes", "[FeatureValidator]") {
    auto tensor = makeTimeTensor(10, 3);
    auto result = MLCore::validateFeatureTensor(tensor);

    CHECK(result.valid);
    CHECK(result.reason == MLCore::RowCompatibility::Compatible);
}

TEST_CASE("FeatureValidator: valid 2D interval tensor passes", "[FeatureValidator]") {
    auto tensor = makeIntervalTensor(5, 3);
    auto result = MLCore::validateFeatureTensor(tensor);

    CHECK(result.valid);
    CHECK(result.reason == MLCore::RowCompatibility::Compatible);
}

TEST_CASE("FeatureValidator: 1D tensor fails validation", "[FeatureValidator]") {
    // Create a 1D tensor via createND
    std::vector<float> data = {1.0f, 2.0f, 3.0f};
    std::vector<AxisDescriptor> axes = {{"values", 3}};
    auto tensor = TensorData::createND(data, axes);

    auto result = MLCore::validateFeatureTensor(tensor);

    CHECK_FALSE(result.valid);
    CHECK(result.reason == MLCore::RowCompatibility::Not2D);
}

TEST_CASE("FeatureValidator: 3D tensor fails validation", "[FeatureValidator]") {
    // Create a 3D tensor
    std::vector<float> data(2 * 3 * 4, 1.0f);
    std::vector<AxisDescriptor> axes = {{"batch", 2}, {"rows", 3}, {"cols", 4}};
    auto tensor = TensorData::createND(data, axes);

    auto result = MLCore::validateFeatureTensor(tensor);

    CHECK_FALSE(result.valid);
    CHECK(result.reason == MLCore::RowCompatibility::Not2D);
}

// ============================================================================
// toString
// ============================================================================

TEST_CASE("FeatureValidator: toString covers all enum values", "[FeatureValidator]") {
    CHECK(MLCore::toString(MLCore::RowCompatibility::Compatible) == "Compatible");
    CHECK(MLCore::toString(MLCore::RowCompatibility::EmptyTensor).find("Empty") != std::string::npos);
    CHECK(MLCore::toString(MLCore::RowCompatibility::Not2D).find("2-dimensional") != std::string::npos);
    CHECK(MLCore::toString(MLCore::RowCompatibility::NoColumns).find("zero columns") != std::string::npos);
    CHECK(MLCore::toString(MLCore::RowCompatibility::RowTypeMismatch).find("mismatch") != std::string::npos);
    CHECK(MLCore::toString(MLCore::RowCompatibility::TimeFrameMismatch).find("mismatch") != std::string::npos);
    CHECK(MLCore::toString(MLCore::RowCompatibility::RowCountMismatch).find("mismatch") != std::string::npos);
}

// ============================================================================
// validateFeatureLabelCompatibility — TimeGroups label source
// ============================================================================

TEST_CASE("FeatureValidator: compatible time-indexed features with TimeGroups labels", "[FeatureValidator]") {
    auto tensor = makeTimeTensor(100, 3);
    MLCore::LabelSourceTimeGroups label_source{100, true};

    auto result = MLCore::validateFeatureLabelCompatibility(tensor, label_source);

    CHECK(result.valid);
    CHECK(result.reason == MLCore::RowCompatibility::Compatible);
    CHECK(result.feature_rows == 100);
    CHECK(result.feature_cols == 3);
    CHECK(result.label_count == 100);
}

TEST_CASE("FeatureValidator: row count mismatch with TimeGroups labels", "[FeatureValidator]") {
    auto tensor = makeTimeTensor(100, 3);
    MLCore::LabelSourceTimeGroups label_source{50, true};  // only 50 labels

    auto result = MLCore::validateFeatureLabelCompatibility(tensor, label_source);

    CHECK_FALSE(result.valid);
    CHECK(result.reason == MLCore::RowCompatibility::RowCountMismatch);
    CHECK(result.feature_rows == 100);
    CHECK(result.label_count == 50);
}

TEST_CASE("FeatureValidator: interval tensor incompatible with TimeGroups labels", "[FeatureValidator]") {
    auto tensor = makeIntervalTensor(10, 3);
    MLCore::LabelSourceTimeGroups label_source{10, true};

    auto result = MLCore::validateFeatureLabelCompatibility(tensor, label_source);

    CHECK_FALSE(result.valid);
    CHECK(result.reason == MLCore::RowCompatibility::RowTypeMismatch);
}

// ============================================================================
// validateFeatureLabelCompatibility — Intervals label source
// ============================================================================

TEST_CASE("FeatureValidator: compatible time-indexed features with Intervals labels", "[FeatureValidator]") {
    auto tensor = makeTimeTensor(100, 3);
    MLCore::LabelSourceIntervals label_source{100, true};

    auto result = MLCore::validateFeatureLabelCompatibility(tensor, label_source);

    CHECK(result.valid);
    CHECK(result.reason == MLCore::RowCompatibility::Compatible);
}

TEST_CASE("FeatureValidator: ordinal features compatible with Intervals labels", "[FeatureValidator]") {
    auto tensor = makeOrdinalTensor(50, 4);
    MLCore::LabelSourceIntervals label_source{50, false};

    auto result = MLCore::validateFeatureLabelCompatibility(tensor, label_source);

    CHECK(result.valid);
}

TEST_CASE("FeatureValidator: interval tensor incompatible with Intervals labels", "[FeatureValidator]") {
    auto tensor = makeIntervalTensor(10, 3);
    MLCore::LabelSourceIntervals label_source{10, true};

    auto result = MLCore::validateFeatureLabelCompatibility(tensor, label_source);

    CHECK_FALSE(result.valid);
    CHECK(result.reason == MLCore::RowCompatibility::RowTypeMismatch);
}

// ============================================================================
// validateFeatureLabelCompatibility — EntityGroups label source
// ============================================================================

TEST_CASE("FeatureValidator: entity groups compatible with any row type", "[FeatureValidator]") {
    MLCore::LabelSourceEntityGroups label_source{10};

    auto ordinal = makeOrdinalTensor(10, 3);
    CHECK(MLCore::validateFeatureLabelCompatibility(ordinal, label_source).valid);

    auto time_tensor = makeTimeTensor(10, 3);
    CHECK(MLCore::validateFeatureLabelCompatibility(time_tensor, label_source).valid);

    auto interval_tensor = makeIntervalTensor(10, 3);
    CHECK(MLCore::validateFeatureLabelCompatibility(interval_tensor, label_source).valid);
}

TEST_CASE("FeatureValidator: entity groups row count mismatch", "[FeatureValidator]") {
    auto tensor = makeOrdinalTensor(10, 3);
    MLCore::LabelSourceEntityGroups label_source{20};  // mismatch

    auto result = MLCore::validateFeatureLabelCompatibility(tensor, label_source);

    CHECK_FALSE(result.valid);
    CHECK(result.reason == MLCore::RowCompatibility::RowCountMismatch);
}

// ============================================================================
// validateFeatureLabelCompatibility — empty tensor propagates correctly
// ============================================================================

TEST_CASE("FeatureValidator: empty tensor fails label validation too", "[FeatureValidator]") {
    TensorData empty;
    MLCore::LabelSourceTimeGroups label_source{0, false};

    auto result = MLCore::validateFeatureLabelCompatibility(empty, label_source);

    CHECK_FALSE(result.valid);
    CHECK(result.reason == MLCore::RowCompatibility::EmptyTensor);
}

// ============================================================================
// countNonFiniteRows / findNonFiniteRows
// ============================================================================

TEST_CASE("FeatureValidator: no NaN rows in clean tensor", "[FeatureValidator]") {
    auto tensor = makeOrdinalTensor(10, 3);

    CHECK(MLCore::countNonFiniteRows(tensor) == 0);
    CHECK(MLCore::findNonFiniteRows(tensor).empty());
}

TEST_CASE("FeatureValidator: detects NaN rows", "[FeatureValidator]") {
    auto tensor = makeTensorWithNaNs(10, 3, {2, 5, 7});

    CHECK(MLCore::countNonFiniteRows(tensor) == 3);

    auto bad_rows = MLCore::findNonFiniteRows(tensor);
    REQUIRE(bad_rows.size() == 3);
    CHECK(bad_rows[0] == 2);
    CHECK(bad_rows[1] == 5);
    CHECK(bad_rows[2] == 7);
}

TEST_CASE("FeatureValidator: detects Inf rows", "[FeatureValidator]") {
    std::vector<float> data = {
        1.0f, 2.0f,
        std::numeric_limits<float>::infinity(), 4.0f,
        5.0f, 6.0f,
        7.0f, -std::numeric_limits<float>::infinity()
    };
    auto tensor = TensorData::createOrdinal2D(data, 4, 2);

    CHECK(MLCore::countNonFiniteRows(tensor) == 2);

    auto bad_rows = MLCore::findNonFiniteRows(tensor);
    REQUIRE(bad_rows.size() == 2);
    CHECK(bad_rows[0] == 1);
    CHECK(bad_rows[1] == 3);
}

TEST_CASE("FeatureValidator: empty tensor returns zero non-finite rows", "[FeatureValidator]") {
    TensorData empty;
    CHECK(MLCore::countNonFiniteRows(empty) == 0);
    CHECK(MLCore::findNonFiniteRows(empty).empty());
}

TEST_CASE("FeatureValidator: all NaN rows detected", "[FeatureValidator]") {
    auto tensor = makeTensorWithNaNs(5, 2, {0, 1, 2, 3, 4});

    CHECK(MLCore::countNonFiniteRows(tensor) == 5);
    CHECK(MLCore::findNonFiniteRows(tensor).size() == 5);
}
