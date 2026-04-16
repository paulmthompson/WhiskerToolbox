/**
 * @file TensorColumnViewCreator.test.cpp
 * @brief Tests for TensorData ↔ AnalogTimeSeries bridge utilities
 *
 * Tests:
 * - TensorData → AnalogTimeSeries column views (createColumnViewsInDM)
 * - AnalogTimeSeries key group discovery (discoverAnalogKeyGroupsInDM)
 * - AnalogTimeSeries → TensorData population (populateTensorFromAnalogKeysInDM)
 */

#include "AnalogTimeSeries/Analog_Time_Series.hpp"
#include "DataManager/DataManager.hpp"
#include "Tensors/TensorData.hpp"
#include "TimeFrame/StrongKeyTypes.hpp"
#include "TimeFrame/TimeFrame.hpp"
#include "TimeFrame/TimeIndexStorage.hpp"
#include "TransformsV2/algorithms/AnalogToTensor/AnalogToTensor.hpp"
#include "TransformsV2/algorithms/TensorToAnalog/TensorToAnalog.hpp"
#include "TransformsV2/core/ComputeContext.hpp"

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

#include <algorithm>
#include <cstddef>
#include <map>
#include <memory>
#include <numeric>
#include <string>
#include <vector>

using namespace WhiskerToolbox::Transforms::V2;
using namespace WhiskerToolbox::Transforms::V2::Examples;

// ============================================================================
// Helpers
// ============================================================================

namespace {

/// Create a 2D TensorData with known column values for testing
auto makeTensor(std::size_t rows, std::size_t cols,
                std::shared_ptr<TimeFrame> tf,
                std::vector<std::string> col_names = {})
        -> std::shared_ptr<TensorData> {

    // Fill with value = row * 100 + col (easy to verify)
    std::vector<float> flat(rows * cols);
    for (std::size_t r = 0; r < rows; ++r) {
        for (std::size_t c = 0; c < cols; ++c) {
            flat[r * cols + c] = static_cast<float>(r * 100 + c);
        }
    }

    auto time_storage = TimeIndexStorageFactory::createDenseFromZero(rows);

    auto tensor = TensorData::createTimeSeries2D(
            flat, rows, cols, time_storage, tf, std::move(col_names));
    return std::make_shared<TensorData>(std::move(tensor));
}

/// Insert tensor into a DataManager and run tensorToAnalog, inserting results
auto createColumnViewsInDM(
        DataManager & dm,
        std::string const & tensor_key,
        std::string const & prefix,
        std::vector<int> const & columns) -> std::size_t {

    auto tensor = dm.getData<TensorData>(tensor_key);
    if (!tensor || tensor->ndim() != 2) {
        return 0;
    }

    TensorToAnalogParams params;
    params.columns = columns;

    ComputeContext const ctx;
    auto views = tensorToAnalog(*tensor, params, ctx);
    if (views.empty()) {
        return 0;
    }

    auto const & col_names = tensor->columnNames();
    auto const time_key = dm.getTimeKey(tensor_key);

    std::vector<int> actual_columns = columns;
    if (actual_columns.empty()) {
        actual_columns.resize(tensor->numColumns());
        std::iota(actual_columns.begin(), actual_columns.end(), 0);
    }

    std::size_t count = 0;
    for (std::size_t i = 0; i < views.size(); ++i) {
        auto const col_idx = static_cast<std::size_t>(actual_columns[i]);

        std::string suffix;
        if (col_idx < col_names.size() && !col_names[col_idx].empty()) {
            suffix = col_names[col_idx];
        } else {
            suffix = "ch" + std::to_string(col_idx);
        }

        auto key = prefix;
        key += "/";
        key += suffix;
        dm.setData<AnalogTimeSeries>(key, views[i], time_key);
        ++count;
    }
    return count;
}

/// Mirrors discoverAnalogKeyGroups() from TensorColumnViewCreator.cpp
struct TestAnalogKeyGroup {
    std::string prefix;
    std::vector<std::string> keys;
};

auto discoverAnalogKeyGroupsInDM(DataManager & dm) -> std::vector<TestAnalogKeyGroup> {
    auto const analog_keys = dm.getKeys<AnalogTimeSeries>();

    std::map<std::string, std::vector<std::string>> groups;
    for (auto const & key: analog_keys) {
        auto const pos = key.rfind('_');
        if (pos == std::string::npos || pos == 0) {
            continue;
        }
        auto const prefix = key.substr(0, pos);
        groups[prefix].push_back(key);
    }

    std::vector<TestAnalogKeyGroup> result;
    for (auto & [prefix, keys]: groups) {
        if (keys.size() >= 2) {
            std::sort(keys.begin(), keys.end());
            result.push_back({prefix, std::move(keys)});
        }
    }
    return result;
}

/// Mirrors populateTensorFromAnalogKeys() from TensorColumnViewCreator.cpp
auto populateTensorFromAnalogKeysInDM(
        DataManager & dm,
        std::string const & tensor_key,
        std::vector<std::string> const & analog_keys) -> bool {

    if (analog_keys.empty()) {
        return false;
    }

    std::vector<std::shared_ptr<AnalogTimeSeries const>> channels;
    channels.reserve(analog_keys.size());
    for (auto const & key: analog_keys) {
        auto analog = dm.getData<AnalogTimeSeries>(key);
        if (!analog) {
            return false;
        }
        channels.push_back(std::move(analog));
    }

    using namespace WhiskerToolbox::Transforms::V2::Examples;

    AnalogToTensorParams params;
    params.channel_keys = analog_keys;

    WhiskerToolbox::Transforms::V2::ComputeContext const ctx;
    auto new_tensor = analogToTensor(channels, params, ctx);
    if (!new_tensor) {
        return false;
    }

    auto const time_key = dm.getTimeKey(analog_keys.front());
    dm.setData<TensorData>(tensor_key, new_tensor, time_key);
    return true;
}

/// Helper to create an AnalogTimeSeries with given values
auto makeAnalog(std::vector<float> values,
                std::shared_ptr<TimeFrame> tf)
        -> std::shared_ptr<AnalogTimeSeries> {
    auto const n = values.size();
    auto analog = std::make_shared<AnalogTimeSeries>(std::move(values), n);
    analog->setTimeFrame(std::move(tf));
    return analog;
}

}// anonymous namespace

// ============================================================================
// Tests
// ============================================================================

TEST_CASE("TensorColumnViews - all columns created in DataManager",
          "[TensorColumnViews][DataManager]") {

    auto tf = std::make_shared<TimeFrame>(std::vector<int>{0, 1, 2, 3, 4});
    auto tensor = makeTensor(5, 3, tf, {"alpha", "beta", "gamma"});

    DataManager dm;
    dm.setData<TensorData>("my_tensor", tensor, TimeKey{"time"});

    auto const count = createColumnViewsInDM(dm, "my_tensor", "views", {});

    REQUIRE(count == 3);

    auto alpha = dm.getData<AnalogTimeSeries>("views/alpha");
    auto beta = dm.getData<AnalogTimeSeries>("views/beta");
    auto gamma = dm.getData<AnalogTimeSeries>("views/gamma");

    REQUIRE(alpha != nullptr);
    REQUIRE(beta != nullptr);
    REQUIRE(gamma != nullptr);

    // Verify values: value = row * 100 + col
    REQUIRE(alpha->getAnalogTimeSeries().size() == 5);
    CHECK_THAT(alpha->getAnalogTimeSeries()[0], Catch::Matchers::WithinAbs(0.0, 1e-5));  // row0, col0
    CHECK_THAT(alpha->getAnalogTimeSeries()[2], Catch::Matchers::WithinAbs(200.0, 1e-5));// row2, col0
    CHECK_THAT(beta->getAnalogTimeSeries()[1], Catch::Matchers::WithinAbs(101.0, 1e-5)); // row1, col1
    CHECK_THAT(gamma->getAnalogTimeSeries()[3], Catch::Matchers::WithinAbs(302.0, 1e-5));// row3, col2
}

TEST_CASE("TensorColumnViews - subset of columns",
          "[TensorColumnViews][DataManager]") {

    auto tf = std::make_shared<TimeFrame>(std::vector<int>{0, 1, 2});
    auto tensor = makeTensor(3, 4, tf, {"a", "b", "c", "d"});

    DataManager dm;
    dm.setData<TensorData>("t", tensor, TimeKey{"time"});

    auto const count = createColumnViewsInDM(dm, "t", "sub", {1, 3});

    REQUIRE(count == 2);
    REQUIRE(dm.getData<AnalogTimeSeries>("sub/b") != nullptr);
    REQUIRE(dm.getData<AnalogTimeSeries>("sub/d") != nullptr);
    REQUIRE(dm.getData<AnalogTimeSeries>("sub/a") == nullptr);
    REQUIRE(dm.getData<AnalogTimeSeries>("sub/c") == nullptr);
}

TEST_CASE("TensorColumnViews - fallback to chN naming without column names",
          "[TensorColumnViews][DataManager]") {

    auto tf = std::make_shared<TimeFrame>(std::vector<int>{0, 1});
    auto tensor = makeTensor(2, 2, tf);

    DataManager dm;
    dm.setData<TensorData>("t", tensor, TimeKey{"time"});

    auto const count = createColumnViewsInDM(dm, "t", "unnamed", {});

    REQUIRE(count == 2);
    REQUIRE(dm.getData<AnalogTimeSeries>("unnamed/ch0") != nullptr);
    REQUIRE(dm.getData<AnalogTimeSeries>("unnamed/ch1") != nullptr);
}

TEST_CASE("TensorColumnViews - nonexistent tensor key returns 0",
          "[TensorColumnViews][DataManager]") {

    DataManager dm;
    auto const count = createColumnViewsInDM(dm, "missing", "prefix", {});
    REQUIRE(count == 0);
}

// ============================================================================
// discoverAnalogKeyGroups tests
// ============================================================================

TEST_CASE("discoverAnalogKeyGroups - groups keys by prefix before last underscore",
          "[AnalogKeyGroups][DataManager]") {

    auto tf = std::make_shared<TimeFrame>(std::vector<int>{0, 1, 2});

    DataManager dm;
    dm.setData<AnalogTimeSeries>("voltage_1", makeAnalog({1.0f, 2.0f, 3.0f}, tf), TimeKey{"time"});
    dm.setData<AnalogTimeSeries>("voltage_2", makeAnalog({4.0f, 5.0f, 6.0f}, tf), TimeKey{"time"});
    dm.setData<AnalogTimeSeries>("voltage_3", makeAnalog({7.0f, 8.0f, 9.0f}, tf), TimeKey{"time"});
    dm.setData<AnalogTimeSeries>("current_a", makeAnalog({1.0f, 1.0f, 1.0f}, tf), TimeKey{"time"});
    dm.setData<AnalogTimeSeries>("current_b", makeAnalog({2.0f, 2.0f, 2.0f}, tf), TimeKey{"time"});
    dm.setData<AnalogTimeSeries>("singleton", makeAnalog({0.0f, 0.0f, 0.0f}, tf), TimeKey{"time"});

    auto groups = discoverAnalogKeyGroupsInDM(dm);

    // "singleton" has no underscore → excluded
    // "voltage" has 3 keys, "current" has 2 keys
    REQUIRE(groups.size() == 2);

    // Groups are sorted by prefix (map ordering)
    CHECK(groups[0].prefix == "current");
    CHECK(groups[0].keys.size() == 2);
    CHECK(groups[1].prefix == "voltage");
    CHECK(groups[1].keys.size() == 3);

    // Keys within each group are sorted
    CHECK(groups[0].keys[0] == "current_a");
    CHECK(groups[0].keys[1] == "current_b");
    CHECK(groups[1].keys[0] == "voltage_1");
    CHECK(groups[1].keys[1] == "voltage_2");
    CHECK(groups[1].keys[2] == "voltage_3");
}

TEST_CASE("discoverAnalogKeyGroups - empty DataManager returns no groups",
          "[AnalogKeyGroups][DataManager]") {

    DataManager dm;
    auto groups = discoverAnalogKeyGroupsInDM(dm);
    REQUIRE(groups.empty());
}

TEST_CASE("discoverAnalogKeyGroups - all singletons returns no groups",
          "[AnalogKeyGroups][DataManager]") {

    auto tf = std::make_shared<TimeFrame>(std::vector<int>{0, 1});

    DataManager dm;
    dm.setData<AnalogTimeSeries>("alpha_1", makeAnalog({1.0f, 2.0f}, tf), TimeKey{"time"});
    dm.setData<AnalogTimeSeries>("beta_1", makeAnalog({3.0f, 4.0f}, tf), TimeKey{"time"});
    dm.setData<AnalogTimeSeries>("gamma_1", makeAnalog({5.0f, 6.0f}, tf), TimeKey{"time"});

    auto groups = discoverAnalogKeyGroupsInDM(dm);

    // Each prefix has only 1 key → no groups
    REQUIRE(groups.empty());
}

// ============================================================================
// populateTensorFromAnalogKeys tests
// ============================================================================

TEST_CASE("populateTensorFromAnalogKeys - populates empty tensor from analog channels",
          "[PopulateTensor][DataManager]") {

    auto tf = std::make_shared<TimeFrame>(std::vector<int>{0, 1, 2, 3});

    DataManager dm;
    dm.setData<AnalogTimeSeries>("v_0", makeAnalog({1.0f, 2.0f, 3.0f, 4.0f}, tf), TimeKey{"time"});
    dm.setData<AnalogTimeSeries>("v_1", makeAnalog({10.0f, 20.0f, 30.0f, 40.0f}, tf), TimeKey{"time"});
    dm.setData<TensorData>("tensor", TimeKey{"time"});

    // Tensor starts empty
    auto tensor_before = dm.getData<TensorData>("tensor");
    REQUIRE(tensor_before->isEmpty());

    auto const success = populateTensorFromAnalogKeysInDM(
            dm, "tensor", {"v_0", "v_1"});

    REQUIRE(success);

    auto tensor_after = dm.getData<TensorData>("tensor");
    REQUIRE_FALSE(tensor_after->isEmpty());
    REQUIRE(tensor_after->numRows() == 4);
    REQUIRE(tensor_after->numColumns() == 2);

    // Column names come from the keys
    auto const & col_names = tensor_after->columnNames();
    REQUIRE(col_names.size() == 2);
    CHECK(col_names[0] == "v_0");
    CHECK(col_names[1] == "v_1");
}

TEST_CASE("populateTensorFromAnalogKeys - fails with empty key list",
          "[PopulateTensor][DataManager]") {

    DataManager dm;
    dm.setData<TensorData>("tensor", TimeKey{"time"});

    auto const success = populateTensorFromAnalogKeysInDM(dm, "tensor", {});
    REQUIRE_FALSE(success);
}

TEST_CASE("populateTensorFromAnalogKeys - fails with nonexistent analog key",
          "[PopulateTensor][DataManager]") {

    auto tf = std::make_shared<TimeFrame>(std::vector<int>{0, 1});

    DataManager dm;
    dm.setData<AnalogTimeSeries>("v_0", makeAnalog({1.0f, 2.0f}, tf), TimeKey{"time"});
    dm.setData<TensorData>("tensor", TimeKey{"time"});

    auto const success = populateTensorFromAnalogKeysInDM(
            dm, "tensor", {"v_0", "missing_key"});
    REQUIRE_FALSE(success);

    // Tensor should still be empty (unchanged)
    auto tensor = dm.getData<TensorData>("tensor");
    REQUIRE(tensor->isEmpty());
}
