/**
 * @file TensorCentralDifference.test.cpp
 * @brief Catch2 tests for the TensorCentralDifference container transform
 *
 * Tests boundary policies (NaN, Drop, Clamp, Zero), include_original flag,
 * sparse time indices, single/two row edge cases, rejection of non-TimeFrameIndex
 * row types, JSON parameter round-trip, registry integration, and DataManager
 * pipeline integration.
 */

#include "TransformsV2/algorithms/TensorCentralDifference/TensorCentralDifference.hpp"

#include "DataManager.hpp"
#include "Tensors/RowDescriptor.hpp"
#include "Tensors/TensorData.hpp"
#include "TimeFrame/TimeFrame.hpp"
#include "TimeFrame/TimeIndexStorage.hpp"
#include "TransformsV2/core/ComputeContext.hpp"
#include "TransformsV2/core/DataManagerIntegration.hpp"
#include "TransformsV2/core/ElementRegistry.hpp"
#include "TransformsV2/core/ParameterIO.hpp"

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

#include <cmath>
#include <cstddef>
#include <filesystem>
#include <fstream>
#include <memory>
#include <string>
#include <vector>

using namespace WhiskerToolbox::Transforms::V2;
using namespace WhiskerToolbox::Transforms::V2::Examples;

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

/// 5 rows x 2 cols, dense time storage [0..4], row-major
/// Row i: [10*(i+1), 10*(i+1)+1]  ->  [10,11], [20,21], [30,31], [40,41], [50,51]
TensorData makeSimpleTensor() {
    std::vector<float> const data = {
            10.0f, 11.0f,
            20.0f, 21.0f,
            30.0f, 31.0f,
            40.0f, 41.0f,
            50.0f, 51.0f};
    auto ts = makeDenseTimeStorage(5);
    auto tf = makeTimeFrame(100);
    return TensorData::createTimeSeries2D(data, 5, 2, ts, tf, {"a", "b"});
}

/// 3 rows x 2 cols, sparse time indices [10, 20, 50]
TensorData makeSparseTimeTensor() {
    std::vector<float> const data = {
            100.0f, 101.0f,
            200.0f, 201.0f,
            500.0f, 501.0f};
    std::vector<TimeFrameIndex> const times = {
            TimeFrameIndex{10}, TimeFrameIndex{20}, TimeFrameIndex{50}};
    auto ts = std::make_shared<SparseTimeIndexStorage>(times);
    auto tf = makeTimeFrame(100);
    return TensorData::createTimeSeries2D(data, 3, 2, ts, tf, {"x", "y"});
}

ComputeContext makeCtx() {
    return ComputeContext{};
}

}// namespace

// ============================================================================
// Basic central difference with NaN boundary
// ============================================================================

TEST_CASE("TensorCentralDifference basic NaN boundary", "[TensorCentralDifference]") {
    auto tensor = makeSimpleTensor();
    TensorCentralDifferenceParams params;
    params.boundary_policy = DeltaBoundaryPolicy::NaN;

    auto result = tensorCentralDifference(tensor, params, makeCtx());
    REQUIRE(result != nullptr);

    // 5 rows, original 2 cols + 2 delta cols = 4 cols
    CHECK(result->numRows() == 5);
    CHECK(result->numColumns() == 4);
    CHECK(result->rowType() == RowType::TimeFrameIndex);

    // Column names: a, b, a_delta, b_delta
    auto const & names = result->columnNames();
    REQUIRE(names.size() == 4);
    CHECK(names[0] == "a");
    CHECK(names[1] == "b");
    CHECK(names[2] == "a_delta");
    CHECK(names[3] == "b_delta");

    auto flat = result->materializeFlat();

    // Row 0: original=[10,11], delta=NaN (no t-1)
    CHECK(flat[0 * 4 + 0] == 10.0f);
    CHECK(flat[0 * 4 + 1] == 11.0f);
    CHECK(std::isnan(flat[0 * 4 + 2]));
    CHECK(std::isnan(flat[0 * 4 + 3]));

    // Row 2 (middle): delta = (row3 - row1) / 2 = (40-20)/2=10, (41-21)/2=10
    CHECK(flat[2 * 4 + 0] == 30.0f);
    CHECK(flat[2 * 4 + 1] == 31.0f);
    CHECK(flat[2 * 4 + 2] == 10.0f);
    CHECK(flat[2 * 4 + 3] == 10.0f);

    // Row 4 (last): delta=NaN (no t+1)
    CHECK(flat[4 * 4 + 0] == 50.0f);
    CHECK(flat[4 * 4 + 1] == 51.0f);
    CHECK(std::isnan(flat[4 * 4 + 2]));
    CHECK(std::isnan(flat[4 * 4 + 3]));

    // Row 1: delta = (row2 - row0) / 2 = (30-10)/2=10, (31-11)/2=10
    CHECK(flat[1 * 4 + 2] == 10.0f);
    CHECK(flat[1 * 4 + 3] == 10.0f);

    // Row 3: delta = (row4 - row2) / 2 = (50-30)/2=10, (51-31)/2=10
    CHECK(flat[3 * 4 + 2] == 10.0f);
    CHECK(flat[3 * 4 + 3] == 10.0f);
}

// ============================================================================
// Drop boundary policy
// ============================================================================

TEST_CASE("TensorCentralDifference Drop boundary", "[TensorCentralDifference]") {
    auto tensor = makeSimpleTensor();
    TensorCentralDifferenceParams params;
    params.boundary_policy = DeltaBoundaryPolicy::Drop;

    auto result = tensorCentralDifference(tensor, params, makeCtx());
    REQUIRE(result != nullptr);

    // Only rows 1,2,3 survive (rows 0 and 4 are boundary)
    CHECK(result->numRows() == 3);
    CHECK(result->numColumns() == 4);

    auto flat = result->materializeFlat();

    // Surviving row 0 (originally row 1): original=[20,21], delta=(30-10)/2=10
    CHECK(flat[0 * 4 + 0] == 20.0f);
    CHECK(flat[0 * 4 + 1] == 21.0f);
    CHECK(flat[0 * 4 + 2] == 10.0f);
    CHECK(flat[0 * 4 + 3] == 10.0f);

    // Verify time indices: should be [1, 2, 3]
    auto const & row_desc = result->rows();
    auto const & ts = row_desc.timeStorage();
    CHECK(ts.getTimeFrameIndexAt(0) == TimeFrameIndex{1});
    CHECK(ts.getTimeFrameIndexAt(1) == TimeFrameIndex{2});
    CHECK(ts.getTimeFrameIndexAt(2) == TimeFrameIndex{3});
}

// ============================================================================
// Clamp boundary policy
// ============================================================================

TEST_CASE("TensorCentralDifference Clamp boundary", "[TensorCentralDifference]") {
    auto tensor = makeSimpleTensor();
    TensorCentralDifferenceParams params;
    params.boundary_policy = DeltaBoundaryPolicy::Clamp;

    auto result = tensorCentralDifference(tensor, params, makeCtx());
    REQUIRE(result != nullptr);
    CHECK(result->numRows() == 5);
    CHECK(result->numColumns() == 4);

    auto flat = result->materializeFlat();

    // Row 0: t-1 clamped to row 0 → delta = (row1 - row0) / 2 = (20-10)/2=5, (21-11)/2=5
    CHECK(flat[0 * 4 + 2] == 5.0f);
    CHECK(flat[0 * 4 + 3] == 5.0f);

    // Row 4: t+1 clamped to row 4 → delta = (row4 - row3) / 2 = (50-40)/2=5, (51-41)/2=5
    CHECK(flat[4 * 4 + 2] == 5.0f);
    CHECK(flat[4 * 4 + 3] == 5.0f);

    // Row 2 (middle): standard delta = (40-20)/2=10
    CHECK(flat[2 * 4 + 2] == 10.0f);
    CHECK(flat[2 * 4 + 3] == 10.0f);
}

// ============================================================================
// Zero boundary policy
// ============================================================================

TEST_CASE("TensorCentralDifference Zero boundary", "[TensorCentralDifference]") {
    auto tensor = makeSimpleTensor();
    TensorCentralDifferenceParams params;
    params.boundary_policy = DeltaBoundaryPolicy::Zero;

    auto result = tensorCentralDifference(tensor, params, makeCtx());
    REQUIRE(result != nullptr);
    CHECK(result->numRows() == 5);
    CHECK(result->numColumns() == 4);

    auto flat = result->materializeFlat();

    // Row 0: zero fill
    CHECK(flat[0 * 4 + 2] == 0.0f);
    CHECK(flat[0 * 4 + 3] == 0.0f);

    // Row 1: standard delta = (30-10)/2=10
    CHECK(flat[1 * 4 + 2] == 10.0f);
    CHECK(flat[1 * 4 + 3] == 10.0f);

    // Row 4: zero fill
    CHECK(flat[4 * 4 + 2] == 0.0f);
    CHECK(flat[4 * 4 + 3] == 0.0f);
}

// ============================================================================
// Sparse time indices (index-adjacent, not frame ±1)
// ============================================================================

TEST_CASE("TensorCentralDifference sparse time indices", "[TensorCentralDifference]") {
    auto tensor = makeSparseTimeTensor();// Rows at frames [10, 20, 50]
    TensorCentralDifferenceParams params;
    params.boundary_policy = DeltaBoundaryPolicy::NaN;

    auto result = tensorCentralDifference(tensor, params, makeCtx());
    REQUIRE(result != nullptr);
    CHECK(result->numRows() == 3);
    CHECK(result->numColumns() == 4);

    auto flat = result->materializeFlat();

    // Row 0 (frame 10): delta = NaN (no t-1)
    CHECK(std::isnan(flat[0 * 4 + 2]));
    CHECK(std::isnan(flat[0 * 4 + 3]));

    // Row 1 (frame 20): delta = (row2 - row0) / 2 = (500-100)/2=200, (501-101)/2=200
    CHECK(flat[1 * 4 + 2] == 200.0f);
    CHECK(flat[1 * 4 + 3] == 200.0f);

    // Row 2 (frame 50): delta = NaN (no t+1)
    CHECK(std::isnan(flat[2 * 4 + 2]));
    CHECK(std::isnan(flat[2 * 4 + 3]));
}

// ============================================================================
// Single row tensor
// ============================================================================

TEST_CASE("TensorCentralDifference single row NaN", "[TensorCentralDifference]") {
    std::vector<float> const data = {1.0f, 2.0f};
    auto ts = makeDenseTimeStorage(1);
    auto tf = makeTimeFrame(10);
    auto tensor = TensorData::createTimeSeries2D(data, 1, 2, ts, tf, {"a", "b"});

    TensorCentralDifferenceParams params;
    params.boundary_policy = DeltaBoundaryPolicy::NaN;

    auto result = tensorCentralDifference(tensor, params, makeCtx());
    REQUIRE(result != nullptr);
    CHECK(result->numRows() == 1);
    CHECK(result->numColumns() == 4);

    auto flat = result->materializeFlat();
    CHECK(flat[0] == 1.0f);
    CHECK(flat[1] == 2.0f);
    CHECK(std::isnan(flat[2]));
    CHECK(std::isnan(flat[3]));
}

TEST_CASE("TensorCentralDifference single row Drop returns nullptr", "[TensorCentralDifference]") {
    std::vector<float> const data = {1.0f, 2.0f};
    auto ts = makeDenseTimeStorage(1);
    auto tf = makeTimeFrame(10);
    auto tensor = TensorData::createTimeSeries2D(data, 1, 2, ts, tf, {"a", "b"});

    TensorCentralDifferenceParams params;
    params.boundary_policy = DeltaBoundaryPolicy::Drop;

    auto result = tensorCentralDifference(tensor, params, makeCtx());
    CHECK(result == nullptr);
}

// ============================================================================
// Two row tensor
// ============================================================================

TEST_CASE("TensorCentralDifference two rows Drop returns nullptr", "[TensorCentralDifference]") {
    std::vector<float> const data = {1.0f, 2.0f, 3.0f, 4.0f};
    auto ts = makeDenseTimeStorage(2);
    auto tf = makeTimeFrame(10);
    auto tensor = TensorData::createTimeSeries2D(data, 2, 2, ts, tf, {"a", "b"});

    TensorCentralDifferenceParams params;
    params.boundary_policy = DeltaBoundaryPolicy::Drop;

    auto result = tensorCentralDifference(tensor, params, makeCtx());
    // Both rows are boundary rows → all dropped
    CHECK(result == nullptr);
}

TEST_CASE("TensorCentralDifference two rows Clamp", "[TensorCentralDifference]") {
    std::vector<float> const data = {1.0f, 2.0f, 5.0f, 8.0f};
    auto ts = makeDenseTimeStorage(2);
    auto tf = makeTimeFrame(10);
    auto tensor = TensorData::createTimeSeries2D(data, 2, 2, ts, tf, {"a", "b"});

    TensorCentralDifferenceParams params;
    params.boundary_policy = DeltaBoundaryPolicy::Clamp;

    auto result = tensorCentralDifference(tensor, params, makeCtx());
    REQUIRE(result != nullptr);
    CHECK(result->numRows() == 2);
    CHECK(result->numColumns() == 4);

    auto flat = result->materializeFlat();

    // Row 0: t-1 clamped to self → delta = (row1 - row0) / 2 = (5-1)/2=2, (8-2)/2=3
    CHECK(flat[0 * 4 + 2] == 2.0f);
    CHECK(flat[0 * 4 + 3] == 3.0f);

    // Row 1: t+1 clamped to self → delta = (row1 - row0) / 2 = (5-1)/2=2, (8-2)/2=3
    CHECK(flat[1 * 4 + 2] == 2.0f);
    CHECK(flat[1 * 4 + 3] == 3.0f);
}

// ============================================================================
// Ordinal row type rejected
// ============================================================================

TEST_CASE("TensorCentralDifference rejects Ordinal rows", "[TensorCentralDifference]") {
    auto tensor = TensorData::createOrdinal2D(
            {1.0f, 2.0f, 3.0f, 4.0f}, 2, 2, {"a", "b"});

    TensorCentralDifferenceParams const params;

    auto result = tensorCentralDifference(tensor, params, makeCtx());
    CHECK(result == nullptr);
}

// ============================================================================
// include_original = false
// ============================================================================

TEST_CASE("TensorCentralDifference exclude original columns", "[TensorCentralDifference]") {
    auto tensor = makeSimpleTensor();
    TensorCentralDifferenceParams params;
    params.boundary_policy = DeltaBoundaryPolicy::Clamp;
    params.include_original = false;

    auto result = tensorCentralDifference(tensor, params, makeCtx());
    REQUIRE(result != nullptr);
    CHECK(result->numRows() == 5);
    // Only delta columns: 2 cols
    CHECK(result->numColumns() == 2);

    auto const & names = result->columnNames();
    CHECK(names[0] == "a_delta");
    CHECK(names[1] == "b_delta");

    auto flat = result->materializeFlat();
    // Row 2 (middle): delta = (40-20)/2=10, (41-21)/2=10
    CHECK(flat[2 * 2 + 0] == 10.0f);
    CHECK(flat[2 * 2 + 1] == 10.0f);
}

// ============================================================================
// Registry Integration
// ============================================================================

TEST_CASE("TensorCentralDifference registry integration", "[TensorCentralDifference]") {
    auto & registry = ElementRegistry::instance();

    SECTION("Registered as container transform") {
        REQUIRE(registry.isContainerTransform("TensorCentralDifference"));
        REQUIRE_FALSE(registry.hasElementTransform("TensorCentralDifference"));
    }

    SECTION("Can retrieve container metadata") {
        auto const * metadata = registry.getContainerMetadata("TensorCentralDifference");
        REQUIRE(metadata != nullptr);
        CHECK(metadata->name == "TensorCentralDifference");
        CHECK(metadata->category == "Feature Engineering");
    }

    SECTION("Discoverable by input type") {
        auto transforms = registry.getContainerTransformsForInputType(typeid(TensorData));
        auto it = std::find(transforms.begin(), transforms.end(), "TensorCentralDifference");
        REQUIRE(it != transforms.end());
    }
}

// ============================================================================
// JSON Parameter Loading / Round-trip
// ============================================================================

TEST_CASE("TensorCentralDifferenceParams JSON loading", "[TensorCentralDifference]") {

    SECTION("Load valid JSON with all fields") {
        std::string const json = R"({
            "boundary_policy": "Drop",
            "include_original": false
        })";

        auto result = loadParametersFromJson<TensorCentralDifferenceParams>(json);
        REQUIRE(result);
        auto const & p = result.value();
        CHECK(p.boundary_policy == DeltaBoundaryPolicy::Drop);
        CHECK(p.include_original == false);
    }

    SECTION("Load empty JSON uses defaults") {
        auto result = loadParametersFromJson<TensorCentralDifferenceParams>("{}");
        REQUIRE(result);
        auto const & p = result.value();
        CHECK(p.boundary_policy == DeltaBoundaryPolicy::NaN);
        CHECK(p.include_original == true);
    }

    SECTION("JSON round-trip preserves values") {
        TensorCentralDifferenceParams original;
        original.boundary_policy = DeltaBoundaryPolicy::Clamp;
        original.include_original = false;

        std::string const json = saveParametersToJson(original);

        auto result = loadParametersFromJson<TensorCentralDifferenceParams>(json);
        REQUIRE(result);
        auto const & recovered = result.value();
        CHECK(recovered.boundary_policy == DeltaBoundaryPolicy::Clamp);
        CHECK(recovered.include_original == false);
    }
}

// ============================================================================
// DataManager Pipeline Integration via load_data_from_json_config_v2
// ============================================================================

TEST_CASE("TensorCentralDifference DataManager pipeline integration",
          "[TensorCentralDifference][json_config]") {

    // Set up DataManager with a TensorData key
    DataManager dm;
    auto time_frame = std::make_shared<TimeFrame>(std::vector<int>{0, 1, 2, 3, 4});
    dm.setTime(TimeKey("default"), time_frame);

    // 5 rows x 2 cols, dense time storage
    std::vector<float> const data = {10.0f, 11.0f, 20.0f, 21.0f, 30.0f, 31.0f, 40.0f, 41.0f, 50.0f, 51.0f};
    auto ts = TimeIndexStorageFactory::createDenseFromZero(5);
    auto tensor = std::make_shared<TensorData>(
            TensorData::createTimeSeries2D(data, 5, 2, ts, time_frame, {"a", "b"}));
    tensor->setTimeFrame(time_frame);
    dm.setData("input_tensor", tensor, TimeKey("default"));

    std::filesystem::path const test_dir =
            std::filesystem::temp_directory_path() / "tensor_central_diff_v2_test";
    std::filesystem::create_directories(test_dir);

    SECTION("Execute pipeline - NaN boundary") {
        char const * json_config = R"([
        {
            "transformations": {
                "metadata": {
                    "name": "Central Difference Pipeline",
                    "version": "2.0"
                },
                "steps": [
                    {
                        "step_id": "1",
                        "transform_name": "TensorCentralDifference",
                        "input_key": "input_tensor",
                        "output_key": "delta_tensor",
                        "parameters": {
                            "boundary_policy": "NaN",
                            "include_original": true
                        }
                    }
                ]
            }
        }
        ])";

        std::filesystem::path const json_filepath = test_dir / "central_diff_nan.json";
        {
            std::ofstream f(json_filepath);
            REQUIRE(f.is_open());
            f << json_config;
        }

        auto info_list = load_data_from_json_config_v2(&dm, json_filepath.string());

        auto result = dm.getData<TensorData>("delta_tensor");
        REQUIRE(result != nullptr);
        CHECK(result->numRows() == 5);
        CHECK(result->numColumns() == 4);

        auto flat = result->materializeFlat();
        // Row 0: delta should be NaN
        CHECK(std::isnan(flat[0 * 4 + 2]));
        CHECK(std::isnan(flat[0 * 4 + 3]));
        // Row 2: delta = (40-20)/2 = 10
        CHECK(flat[2 * 4 + 2] == 10.0f);
        CHECK(flat[2 * 4 + 3] == 10.0f);
    }

    SECTION("Execute pipeline - Drop boundary") {
        char const * json_config = R"([
        {
            "transformations": {
                "metadata": {
                    "name": "Central Difference Drop Pipeline",
                    "version": "2.0"
                },
                "steps": [
                    {
                        "step_id": "1",
                        "transform_name": "TensorCentralDifference",
                        "input_key": "input_tensor",
                        "output_key": "delta_tensor_drop",
                        "parameters": {
                            "boundary_policy": "Drop",
                            "include_original": true
                        }
                    }
                ]
            }
        }
        ])";

        std::filesystem::path const json_filepath = test_dir / "central_diff_drop.json";
        {
            std::ofstream f(json_filepath);
            REQUIRE(f.is_open());
            f << json_config;
        }

        auto info_list = load_data_from_json_config_v2(&dm, json_filepath.string());

        auto result = dm.getData<TensorData>("delta_tensor_drop");
        REQUIRE(result != nullptr);
        // Rows 0 and 4 dropped -> 3 surviving rows
        CHECK(result->numRows() == 3);
        CHECK(result->numColumns() == 4);
    }

    // Cleanup
    try {
        std::filesystem::remove_all(test_dir);
    } catch (...) {
    }
}
