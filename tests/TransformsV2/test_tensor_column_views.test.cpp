/**
 * @file test_tensor_column_views.test.cpp
 * @brief Tests for TensorDesign tensor↔analog DataManager bridge utilities.
 *
 * Tests:
 * - TensorData → AnalogTimeSeries column views (createTensorColumnViews)
 * - AnalogTimeSeries key group discovery (discoverAnalogKeyGroups)
 * - AnalogTimeSeries → TensorData population (populateTensorFromAnalogKeys)
 */

#include "TensorDesign/TensorColumnViews.hpp"

#include "AnalogTimeSeries/Analog_Time_Series.hpp"
#include "DataManager/DataManager.hpp"
#include "Tensors/TensorData.hpp"
#include "TimeFrame/StrongKeyTypes.hpp"
#include "TimeFrame/TimeFrame.hpp"
#include "TimeFrame/TimeIndexStorage.hpp"

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

#include <memory>
#include <string>
#include <utility>
#include <vector>

using namespace Neuralyzer::TensorDesign;

namespace {

auto makeTensor(std::size_t rows, std::size_t cols,
                std::shared_ptr<TimeFrame> const & tf,
                std::vector<std::string> col_names = {})
        -> std::shared_ptr<TensorData> {
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

auto makeAnalog(std::vector<float> values, std::shared_ptr<TimeFrame> const & tf)
        -> std::shared_ptr<AnalogTimeSeries> {
    auto const n = values.size();
    auto analog = std::make_shared<AnalogTimeSeries>(std::move(values), n);
    analog->setTimeFrame(tf);
    return analog;
}

}// namespace

TEST_CASE("TensorColumnViews - all columns created in DataManager",
          "[TensorColumnViews][TensorDesign]") {
    auto tf = std::make_shared<TimeFrame>(std::vector<int>{0, 1, 2, 3, 4});
    auto tensor = makeTensor(5, 3, tf, {"alpha", "beta", "gamma"});

    DataManager dm;
    dm.setData<TensorData>("my_tensor", tensor, TimeKey{"time"});

    auto const count = createTensorColumnViews(dm, "my_tensor", "views", {});
    REQUIRE(count == 3);

    auto alpha = dm.getData<AnalogTimeSeries>("views/alpha");
    auto beta = dm.getData<AnalogTimeSeries>("views/beta");
    auto gamma = dm.getData<AnalogTimeSeries>("views/gamma");

    REQUIRE(alpha != nullptr);
    REQUIRE(beta != nullptr);
    REQUIRE(gamma != nullptr);

    REQUIRE(alpha->getAnalogTimeSeries().size() == 5);
    CHECK_THAT(alpha->getAnalogTimeSeries()[0], Catch::Matchers::WithinAbs(0.0, 1e-5));
    CHECK_THAT(alpha->getAnalogTimeSeries()[2], Catch::Matchers::WithinAbs(200.0, 1e-5));
    CHECK_THAT(beta->getAnalogTimeSeries()[1], Catch::Matchers::WithinAbs(101.0, 1e-5));
    CHECK_THAT(gamma->getAnalogTimeSeries()[3], Catch::Matchers::WithinAbs(302.0, 1e-5));
}

TEST_CASE("TensorColumnViews - subset of columns",
          "[TensorColumnViews][TensorDesign]") {
    auto tf = std::make_shared<TimeFrame>(std::vector<int>{0, 1, 2});
    auto tensor = makeTensor(3, 4, tf, {"a", "b", "c", "d"});

    DataManager dm;
    dm.setData<TensorData>("t", tensor, TimeKey{"time"});

    auto const count = createTensorColumnViews(dm, "t", "sub", {1, 3});
    REQUIRE(count == 2);
    REQUIRE(dm.getData<AnalogTimeSeries>("sub/b") != nullptr);
    REQUIRE(dm.getData<AnalogTimeSeries>("sub/d") != nullptr);
    REQUIRE(dm.getData<AnalogTimeSeries>("sub/a") == nullptr);
    REQUIRE(dm.getData<AnalogTimeSeries>("sub/c") == nullptr);
}

TEST_CASE("TensorColumnViews - fallback to chN naming without column names",
          "[TensorColumnViews][TensorDesign]") {
    auto tf = std::make_shared<TimeFrame>(std::vector<int>{0, 1});
    auto tensor = makeTensor(2, 2, tf);

    DataManager dm;
    dm.setData<TensorData>("t", tensor, TimeKey{"time"});

    auto const count = createTensorColumnViews(dm, "t", "unnamed", {});
    REQUIRE(count == 2);
    REQUIRE(dm.getData<AnalogTimeSeries>("unnamed/ch0") != nullptr);
    REQUIRE(dm.getData<AnalogTimeSeries>("unnamed/ch1") != nullptr);
}

TEST_CASE("TensorColumnViews - nonexistent tensor key returns 0",
          "[TensorColumnViews][TensorDesign]") {
    DataManager dm;
    auto const count = createTensorColumnViews(dm, "missing", "prefix", {});
    REQUIRE(count == 0);
}

TEST_CASE("discoverAnalogKeyGroups - groups keys by prefix before last underscore",
          "[AnalogKeyGroups][TensorDesign]") {
    auto tf = std::make_shared<TimeFrame>(std::vector<int>{0, 1, 2});

    DataManager dm;
    dm.setData<AnalogTimeSeries>("voltage_1", makeAnalog({1.0f, 2.0f, 3.0f}, tf), TimeKey{"time"});
    dm.setData<AnalogTimeSeries>("voltage_2", makeAnalog({4.0f, 5.0f, 6.0f}, tf), TimeKey{"time"});
    dm.setData<AnalogTimeSeries>("voltage_3", makeAnalog({7.0f, 8.0f, 9.0f}, tf), TimeKey{"time"});
    dm.setData<AnalogTimeSeries>("current_a", makeAnalog({1.0f, 1.0f, 1.0f}, tf), TimeKey{"time"});
    dm.setData<AnalogTimeSeries>("current_b", makeAnalog({2.0f, 2.0f, 2.0f}, tf), TimeKey{"time"});
    dm.setData<AnalogTimeSeries>("singleton", makeAnalog({0.0f, 0.0f, 0.0f}, tf), TimeKey{"time"});

    auto groups = discoverAnalogKeyGroups(dm);
    REQUIRE(groups.size() == 2);

    CHECK(groups[0].prefix == "current");
    CHECK(groups[0].keys.size() == 2);
    CHECK(groups[1].prefix == "voltage");
    CHECK(groups[1].keys.size() == 3);

    CHECK(groups[0].keys[0] == "current_a");
    CHECK(groups[0].keys[1] == "current_b");
    CHECK(groups[1].keys[0] == "voltage_1");
    CHECK(groups[1].keys[1] == "voltage_2");
    CHECK(groups[1].keys[2] == "voltage_3");
}

TEST_CASE("discoverAnalogKeyGroups - empty DataManager returns no groups",
          "[AnalogKeyGroups][TensorDesign]") {
    DataManager dm;
    auto groups = discoverAnalogKeyGroups(dm);
    REQUIRE(groups.empty());
}

TEST_CASE("discoverAnalogKeyGroups - all singletons returns no groups",
          "[AnalogKeyGroups][TensorDesign]") {
    auto tf = std::make_shared<TimeFrame>(std::vector<int>{0, 1});

    DataManager dm;
    dm.setData<AnalogTimeSeries>("alpha_1", makeAnalog({1.0f, 2.0f}, tf), TimeKey{"time"});
    dm.setData<AnalogTimeSeries>("beta_1", makeAnalog({3.0f, 4.0f}, tf), TimeKey{"time"});
    dm.setData<AnalogTimeSeries>("gamma_1", makeAnalog({5.0f, 6.0f}, tf), TimeKey{"time"});

    auto groups = discoverAnalogKeyGroups(dm);
    REQUIRE(groups.empty());
}

TEST_CASE("populateTensorFromAnalogKeys - populates empty tensor from analog channels",
          "[PopulateTensor][TensorDesign]") {
    auto tf = std::make_shared<TimeFrame>(std::vector<int>{0, 1, 2, 3});

    DataManager dm;
    dm.setData<AnalogTimeSeries>("v_0", makeAnalog({1.0f, 2.0f, 3.0f, 4.0f}, tf), TimeKey{"time"});
    dm.setData<AnalogTimeSeries>("v_1", makeAnalog({10.0f, 20.0f, 30.0f, 40.0f}, tf), TimeKey{"time"});
    dm.setData<TensorData>("tensor", TimeKey{"time"});

    auto tensor_before = dm.getData<TensorData>("tensor");
    REQUIRE(tensor_before->isEmpty());

    auto const success = populateTensorFromAnalogKeys(dm, "tensor", {"v_0", "v_1"});
    REQUIRE(success);

    auto tensor_after = dm.getData<TensorData>("tensor");
    REQUIRE_FALSE(tensor_after->isEmpty());
    REQUIRE(tensor_after->numRows() == 4);
    REQUIRE(tensor_after->numColumns() == 2);

    auto const & col_names = tensor_after->columnNames();
    REQUIRE(col_names.size() == 2);
    CHECK(col_names[0] == "v_0");
    CHECK(col_names[1] == "v_1");
}

TEST_CASE("populateTensorFromAnalogKeys - fails with empty key list",
          "[PopulateTensor][TensorDesign]") {
    DataManager dm;
    dm.setData<TensorData>("tensor", TimeKey{"time"});

    auto const success = populateTensorFromAnalogKeys(dm, "tensor", {});
    REQUIRE_FALSE(success);
}

TEST_CASE("populateTensorFromAnalogKeys - fails with nonexistent analog key",
          "[PopulateTensor][TensorDesign]") {
    auto tf = std::make_shared<TimeFrame>(std::vector<int>{0, 1});

    DataManager dm;
    dm.setData<AnalogTimeSeries>("v_0", makeAnalog({1.0f, 2.0f}, tf), TimeKey{"time"});
    dm.setData<TensorData>("tensor", TimeKey{"time"});

    auto const success = populateTensorFromAnalogKeys(
            dm, "tensor", {"v_0", "missing_key"});
    REQUIRE_FALSE(success);

    auto tensor = dm.getData<TensorData>("tensor");
    REQUIRE(tensor->isEmpty());
}
