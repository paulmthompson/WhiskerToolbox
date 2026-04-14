/**
 * @file TensorTemporalNeighbors.test.cpp
 * @brief Catch2 tests for the TensorTemporalNeighbors container transform
 *
 * Tests boundary policies (NaN, Drop, Clamp, Zero), buildOffsets(),
 * sparse time indices, identity on zero ranges, rejection of
 * non-TimeFrameIndex row types, JSON parameter round-trip,
 * registry integration, and DataManager pipeline integration.
 */

#include "TransformsV2/algorithms/TensorTemporalNeighbors/TensorTemporalNeighbors.hpp"

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
    std::vector<float> data = {
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
    std::vector<float> data = {
            100.0f, 101.0f,
            200.0f, 201.0f,
            500.0f, 501.0f};
    std::vector<TimeFrameIndex> times = {
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
// buildOffsets tests
// ============================================================================

TEST_CASE("TensorTemporalNeighborParams buildOffsets", "[TensorTemporalNeighbors]") {
    SECTION("lag_range=3 lag_step=1 lead_range=2 lead_step=1") {
        TensorTemporalNeighborParams params;
        params.lag_range = 3;
        params.lag_step = 1;
        params.lead_range = 2;
        params.lead_step = 1;

        auto offsets = params.buildOffsets();
        REQUIRE(offsets.size() == 5);
        CHECK(offsets[0] == -3);
        CHECK(offsets[1] == -2);
        CHECK(offsets[2] == -1);
        CHECK(offsets[3] == 1);
        CHECK(offsets[4] == 2);
    }

    SECTION("lag only with step=2") {
        TensorTemporalNeighborParams params;
        params.lag_range = 6;
        params.lag_step = 2;

        auto offsets = params.buildOffsets();
        REQUIRE(offsets.size() == 3);
        CHECK(offsets[0] == -6);
        CHECK(offsets[1] == -4);
        CHECK(offsets[2] == -2);
    }

    SECTION("lead only with step=3") {
        TensorTemporalNeighborParams params;
        params.lead_range = 9;
        params.lead_step = 3;

        auto offsets = params.buildOffsets();
        REQUIRE(offsets.size() == 3);
        CHECK(offsets[0] == 3);
        CHECK(offsets[1] == 6);
        CHECK(offsets[2] == 9);
    }

    SECTION("zero ranges produce empty offsets") {
        TensorTemporalNeighborParams params;
        auto offsets = params.buildOffsets();
        CHECK(offsets.empty());
    }

    SECTION("lag_range=1 lag_step=1 produces single offset") {
        TensorTemporalNeighborParams params;
        params.lag_range = 1;
        params.lag_step = 1;

        auto offsets = params.buildOffsets();
        REQUIRE(offsets.size() == 1);
        CHECK(offsets[0] == -1);
    }
}

// ============================================================================
// Basic lag/lead with NaN boundary
// ============================================================================

TEST_CASE("TensorTemporalNeighbors basic NaN boundary", "[TensorTemporalNeighbors]") {
    auto tensor = makeSimpleTensor();
    TensorTemporalNeighborParams params;
    params.lag_range = 1;
    params.lag_step = 1;
    params.lead_range = 1;
    params.lead_step = 1;
    params.boundary_policy = BoundaryPolicy::NaN;

    auto result = tensorTemporalNeighbors(tensor, params, makeCtx());
    REQUIRE(result != nullptr);

    // 5 rows, original 2 cols + 2 offsets x 2 cols = 6 cols
    CHECK(result->numRows() == 5);
    CHECK(result->numColumns() == 6);
    CHECK(result->rowType() == RowType::TimeFrameIndex);

    // Column names: a, b, a_lag-1, b_lag-1, a_lag+1, b_lag+1
    auto const & names = result->columnNames();
    REQUIRE(names.size() == 6);
    CHECK(names[0] == "a");
    CHECK(names[1] == "b");
    CHECK(names[2] == "a_lag-1");
    CHECK(names[3] == "b_lag-1");
    CHECK(names[4] == "a_lag+1");
    CHECK(names[5] == "b_lag+1");

    auto flat = result->materializeFlat();

    // Row 0: original=[10,11], lag-1=NaN (out-of-bounds), lag+1=[20,21]
    CHECK(flat[0 * 6 + 0] == 10.0f);
    CHECK(flat[0 * 6 + 1] == 11.0f);
    CHECK(std::isnan(flat[0 * 6 + 2]));
    CHECK(std::isnan(flat[0 * 6 + 3]));
    CHECK(flat[0 * 6 + 4] == 20.0f);
    CHECK(flat[0 * 6 + 5] == 21.0f);

    // Row 2 (middle): lag-1=[20,21], lag+1=[40,41]
    CHECK(flat[2 * 6 + 0] == 30.0f);
    CHECK(flat[2 * 6 + 1] == 31.0f);
    CHECK(flat[2 * 6 + 2] == 20.0f);
    CHECK(flat[2 * 6 + 3] == 21.0f);
    CHECK(flat[2 * 6 + 4] == 40.0f);
    CHECK(flat[2 * 6 + 5] == 41.0f);

    // Row 4 (last): lag-1=[40,41], lag+1=NaN
    CHECK(flat[4 * 6 + 0] == 50.0f);
    CHECK(flat[4 * 6 + 1] == 51.0f);
    CHECK(flat[4 * 6 + 2] == 40.0f);
    CHECK(flat[4 * 6 + 3] == 41.0f);
    CHECK(std::isnan(flat[4 * 6 + 4]));
    CHECK(std::isnan(flat[4 * 6 + 5]));
}

// ============================================================================
// Drop boundary policy
// ============================================================================

TEST_CASE("TensorTemporalNeighbors Drop boundary", "[TensorTemporalNeighbors]") {
    auto tensor = makeSimpleTensor();
    TensorTemporalNeighborParams params;
    params.lag_range = 1;
    params.lag_step = 1;
    params.lead_range = 1;
    params.lead_step = 1;
    params.boundary_policy = BoundaryPolicy::Drop;

    auto result = tensorTemporalNeighbors(tensor, params, makeCtx());
    REQUIRE(result != nullptr);

    // Only rows 1,2,3 survive (rows 0 and 4 have out-of-bounds neighbors)
    CHECK(result->numRows() == 3);
    CHECK(result->numColumns() == 6);

    auto flat = result->materializeFlat();

    // Surviving row 0 (originally row 1): original=[20,21], lag-1=[10,11], lag+1=[30,31]
    CHECK(flat[0 * 6 + 0] == 20.0f);
    CHECK(flat[0 * 6 + 1] == 21.0f);
    CHECK(flat[0 * 6 + 2] == 10.0f);
    CHECK(flat[0 * 6 + 3] == 11.0f);
    CHECK(flat[0 * 6 + 4] == 30.0f);
    CHECK(flat[0 * 6 + 5] == 31.0f);

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

TEST_CASE("TensorTemporalNeighbors Clamp boundary", "[TensorTemporalNeighbors]") {
    auto tensor = makeSimpleTensor();
    TensorTemporalNeighborParams params;
    params.lag_range = 1;
    params.lag_step = 1;
    params.lead_range = 1;
    params.lead_step = 1;
    params.boundary_policy = BoundaryPolicy::Clamp;

    auto result = tensorTemporalNeighbors(tensor, params, makeCtx());
    REQUIRE(result != nullptr);
    CHECK(result->numRows() == 5);
    CHECK(result->numColumns() == 6);

    auto flat = result->materializeFlat();

    // Row 0: lag-1 clamped to row 0 itself -> [10,11]
    CHECK(flat[0 * 6 + 2] == 10.0f);
    CHECK(flat[0 * 6 + 3] == 11.0f);
    CHECK(flat[0 * 6 + 4] == 20.0f);
    CHECK(flat[0 * 6 + 5] == 21.0f);

    // Row 4: lag+1 clamped to row 4 itself -> [50,51]
    CHECK(flat[4 * 6 + 2] == 40.0f);
    CHECK(flat[4 * 6 + 3] == 41.0f);
    CHECK(flat[4 * 6 + 4] == 50.0f);
    CHECK(flat[4 * 6 + 5] == 51.0f);
}

// ============================================================================
// Zero boundary policy
// ============================================================================

TEST_CASE("TensorTemporalNeighbors Zero boundary", "[TensorTemporalNeighbors]") {
    auto tensor = makeSimpleTensor();
    TensorTemporalNeighborParams params;
    params.lag_range = 1;
    params.lag_step = 1;
    params.boundary_policy = BoundaryPolicy::Zero;

    auto result = tensorTemporalNeighbors(tensor, params, makeCtx());
    REQUIRE(result != nullptr);
    CHECK(result->numRows() == 5);
    // 2 original + 1 offset x 2 cols = 4
    CHECK(result->numColumns() == 4);

    auto flat = result->materializeFlat();

    // Row 0: lag-1 = [0, 0]
    CHECK(flat[0 * 4 + 2] == 0.0f);
    CHECK(flat[0 * 4 + 3] == 0.0f);

    // Row 1: lag-1 = [10, 11]
    CHECK(flat[1 * 4 + 2] == 10.0f);
    CHECK(flat[1 * 4 + 3] == 11.0f);
}

// ============================================================================
// Multiple offsets via ranges
// ============================================================================

TEST_CASE("TensorTemporalNeighbors multiple offsets", "[TensorTemporalNeighbors]") {
    auto tensor = makeSimpleTensor();
    TensorTemporalNeighborParams params;
    params.lag_range = 2;
    params.lag_step = 1;
    params.lead_range = 2;
    params.lead_step = 1;
    params.boundary_policy = BoundaryPolicy::NaN;

    auto result = tensorTemporalNeighbors(tensor, params, makeCtx());
    REQUIRE(result != nullptr);

    // 2 original + 4 offsets x 2 cols = 10
    CHECK(result->numRows() == 5);
    CHECK(result->numColumns() == 10);

    auto flat = result->materializeFlat();

    // Row 2 (middle): all offsets in-bounds
    // lag-2=[10,11], lag-1=[20,21], lag+1=[40,41], lag+2=[50,51]
    CHECK(flat[2 * 10 + 2] == 10.0f);// a_lag-2
    CHECK(flat[2 * 10 + 3] == 11.0f);// b_lag-2
    CHECK(flat[2 * 10 + 4] == 20.0f);// a_lag-1
    CHECK(flat[2 * 10 + 5] == 21.0f);// b_lag-1
    CHECK(flat[2 * 10 + 6] == 40.0f);// a_lag+1
    CHECK(flat[2 * 10 + 7] == 41.0f);// b_lag+1
    CHECK(flat[2 * 10 + 8] == 50.0f);// a_lag+2
    CHECK(flat[2 * 10 + 9] == 51.0f);// b_lag+2
}

// ============================================================================
// Step size > 1
// ============================================================================

TEST_CASE("TensorTemporalNeighbors step size > 1", "[TensorTemporalNeighbors]") {
    auto tensor = makeSimpleTensor();// 5 rows
    TensorTemporalNeighborParams params;
    params.lag_range = 4;
    params.lag_step = 2;
    params.boundary_policy = BoundaryPolicy::NaN;

    // Offsets: {-4, -2}
    auto offsets = params.buildOffsets();
    REQUIRE(offsets.size() == 2);
    CHECK(offsets[0] == -4);
    CHECK(offsets[1] == -2);

    auto result = tensorTemporalNeighbors(tensor, params, makeCtx());
    REQUIRE(result != nullptr);
    // 2 original + 2 offsets x 2 cols = 6
    CHECK(result->numColumns() == 6);

    auto flat = result->materializeFlat();

    // Row 4: lag-4=row0=[10,11], lag-2=row2=[30,31]
    CHECK(flat[4 * 6 + 2] == 10.0f);
    CHECK(flat[4 * 6 + 3] == 11.0f);
    CHECK(flat[4 * 6 + 4] == 30.0f);
    CHECK(flat[4 * 6 + 5] == 31.0f);
}

// ============================================================================
// Single row tensor
// ============================================================================

TEST_CASE("TensorTemporalNeighbors single row NaN", "[TensorTemporalNeighbors]") {
    std::vector<float> data = {1.0f, 2.0f};
    auto ts = makeDenseTimeStorage(1);
    auto tf = makeTimeFrame(10);
    auto tensor = TensorData::createTimeSeries2D(data, 1, 2, ts, tf, {"a", "b"});

    TensorTemporalNeighborParams params;
    params.lag_range = 1;
    params.lag_step = 1;
    params.lead_range = 1;
    params.lead_step = 1;
    params.boundary_policy = BoundaryPolicy::NaN;

    auto result = tensorTemporalNeighbors(tensor, params, makeCtx());
    REQUIRE(result != nullptr);
    CHECK(result->numRows() == 1);
    CHECK(result->numColumns() == 6);

    auto flat = result->materializeFlat();
    CHECK(flat[0] == 1.0f);
    CHECK(flat[1] == 2.0f);
    CHECK(std::isnan(flat[2]));
    CHECK(std::isnan(flat[3]));
    CHECK(std::isnan(flat[4]));
    CHECK(std::isnan(flat[5]));
}

TEST_CASE("TensorTemporalNeighbors single row Drop returns nullptr", "[TensorTemporalNeighbors]") {
    std::vector<float> data = {1.0f, 2.0f};
    auto ts = makeDenseTimeStorage(1);
    auto tf = makeTimeFrame(10);
    auto tensor = TensorData::createTimeSeries2D(data, 1, 2, ts, tf, {"a", "b"});

    TensorTemporalNeighborParams params;
    params.lag_range = 1;
    params.lag_step = 1;
    params.boundary_policy = BoundaryPolicy::Drop;

    auto result = tensorTemporalNeighbors(tensor, params, makeCtx());
    CHECK(result == nullptr);
}

// ============================================================================
// Sparse time indices (index-adjacent, not frame +/-1)
// ============================================================================

TEST_CASE("TensorTemporalNeighbors sparse time indices", "[TensorTemporalNeighbors]") {
    auto tensor = makeSparseTimeTensor();// Rows at frames [10, 20, 50]
    TensorTemporalNeighborParams params;
    params.lead_range = 1;
    params.lead_step = 1;
    params.boundary_policy = BoundaryPolicy::NaN;

    auto result = tensorTemporalNeighbors(tensor, params, makeCtx());
    REQUIRE(result != nullptr);
    CHECK(result->numRows() == 3);

    auto flat = result->materializeFlat();

    // Row 0 (frame 10): lag+1 = row 1 (frame 20), NOT frame 11
    CHECK(flat[0 * 4 + 2] == 200.0f);
    CHECK(flat[0 * 4 + 3] == 201.0f);

    // Row 1 (frame 20): lag+1 = row 2 (frame 50), NOT frame 21
    CHECK(flat[1 * 4 + 2] == 500.0f);
    CHECK(flat[1 * 4 + 3] == 501.0f);

    // Row 2 (frame 50): lag+1 = NaN
    CHECK(std::isnan(flat[2 * 4 + 2]));
    CHECK(std::isnan(flat[2 * 4 + 3]));
}

// ============================================================================
// Zero ranges -> identity
// ============================================================================

TEST_CASE("TensorTemporalNeighbors zero ranges returns identity", "[TensorTemporalNeighbors]") {
    auto tensor = makeSimpleTensor();
    TensorTemporalNeighborParams params;
    // lag_range=0, lead_range=0 -> no offsets

    auto result = tensorTemporalNeighbors(tensor, params, makeCtx());
    REQUIRE(result != nullptr);
    CHECK(result->numRows() == 5);
    CHECK(result->numColumns() == 2);

    // Data should match original
    auto orig_flat = tensor.materializeFlat();
    auto result_flat = result->materializeFlat();
    for (std::size_t i = 0; i < orig_flat.size(); ++i) {
        CHECK(result_flat[i] == orig_flat[i]);
    }
}

// ============================================================================
// Ordinal row type rejected
// ============================================================================

TEST_CASE("TensorTemporalNeighbors rejects Ordinal rows", "[TensorTemporalNeighbors]") {
    auto tensor = TensorData::createOrdinal2D(
            {1.0f, 2.0f, 3.0f, 4.0f}, 2, 2, {"a", "b"});

    TensorTemporalNeighborParams params;
    params.lag_range = 1;
    params.lag_step = 1;

    auto result = tensorTemporalNeighbors(tensor, params, makeCtx());
    CHECK(result == nullptr);
}

// ============================================================================
// include_original = false
// ============================================================================

TEST_CASE("TensorTemporalNeighbors exclude original columns", "[TensorTemporalNeighbors]") {
    auto tensor = makeSimpleTensor();
    TensorTemporalNeighborParams params;
    params.lag_range = 1;
    params.lag_step = 1;
    params.lead_range = 1;
    params.lead_step = 1;
    params.boundary_policy = BoundaryPolicy::Clamp;
    params.include_original = false;

    auto result = tensorTemporalNeighbors(tensor, params, makeCtx());
    REQUIRE(result != nullptr);
    CHECK(result->numRows() == 5);
    // Only offset columns: 2 offsets x 2 cols = 4
    CHECK(result->numColumns() == 4);

    auto const & names = result->columnNames();
    CHECK(names[0] == "a_lag-1");
    CHECK(names[1] == "b_lag-1");
    CHECK(names[2] == "a_lag+1");
    CHECK(names[3] == "b_lag+1");

    auto flat = result->materializeFlat();
    // Row 2: lag-1=[20,21], lag+1=[40,41]
    CHECK(flat[2 * 4 + 0] == 20.0f);
    CHECK(flat[2 * 4 + 1] == 21.0f);
    CHECK(flat[2 * 4 + 2] == 40.0f);
    CHECK(flat[2 * 4 + 3] == 41.0f);
}

// ============================================================================
// Registry Integration
// ============================================================================

TEST_CASE("TensorTemporalNeighbors registry integration", "[TensorTemporalNeighbors]") {
    auto & registry = ElementRegistry::instance();

    SECTION("Registered as container transform") {
        REQUIRE(registry.isContainerTransform("TensorTemporalNeighbors"));
        REQUIRE_FALSE(registry.hasElementTransform("TensorTemporalNeighbors"));
    }

    SECTION("Can retrieve container metadata") {
        auto const * metadata = registry.getContainerMetadata("TensorTemporalNeighbors");
        REQUIRE(metadata != nullptr);
        CHECK(metadata->name == "TensorTemporalNeighbors");
        CHECK(metadata->category == "Feature Engineering");
    }

    SECTION("Discoverable by input type") {
        auto transforms = registry.getContainerTransformsForInputType(typeid(TensorData));
        auto it = std::find(transforms.begin(), transforms.end(), "TensorTemporalNeighbors");
        REQUIRE(it != transforms.end());
    }
}

// ============================================================================
// JSON Parameter Loading / Round-trip
// ============================================================================

TEST_CASE("TensorTemporalNeighborParams JSON loading", "[TensorTemporalNeighbors]") {

    SECTION("Load valid JSON with all fields") {
        std::string json = R"({
            "lag_range": 3,
            "lag_step": 1,
            "lead_range": 2,
            "lead_step": 1,
            "boundary_policy": "Drop",
            "include_original": false
        })";

        auto result = loadParametersFromJson<TensorTemporalNeighborParams>(json);
        REQUIRE(result);
        auto const & p = result.value();
        CHECK(p.lag_range == 3);
        CHECK(p.lag_step == 1);
        CHECK(p.lead_range == 2);
        CHECK(p.lead_step == 1);
        CHECK(p.boundary_policy == BoundaryPolicy::Drop);
        CHECK(p.include_original == false);
    }

    SECTION("Load empty JSON uses defaults") {
        auto result = loadParametersFromJson<TensorTemporalNeighborParams>("{}");
        REQUIRE(result);
        auto const & p = result.value();
        CHECK(p.lag_range == 0);
        CHECK(p.lag_step == 1);
        CHECK(p.lead_range == 0);
        CHECK(p.lead_step == 1);
        CHECK(p.boundary_policy == BoundaryPolicy::NaN);
        CHECK(p.include_original == true);
    }

    SECTION("JSON round-trip preserves values") {
        TensorTemporalNeighborParams original;
        original.lag_range = 5;
        original.lag_step = 2;
        original.lead_range = 3;
        original.lead_step = 1;
        original.boundary_policy = BoundaryPolicy::Clamp;
        original.include_original = false;

        std::string json = saveParametersToJson(original);

        auto result = loadParametersFromJson<TensorTemporalNeighborParams>(json);
        REQUIRE(result);
        auto const & recovered = result.value();
        CHECK(recovered.lag_range == original.lag_range);
        CHECK(recovered.lag_step == original.lag_step);
        CHECK(recovered.lead_range == original.lead_range);
        CHECK(recovered.lead_step == original.lead_step);
        CHECK(recovered.boundary_policy == BoundaryPolicy::Clamp);
        CHECK(recovered.include_original == false);
    }
}

// ============================================================================
// DataManager Pipeline Integration via load_data_from_json_config_v2
// ============================================================================

TEST_CASE("TensorTemporalNeighbors DataManager pipeline integration",
          "[TensorTemporalNeighbors][json_config]") {

    // Set up DataManager with a TensorData key
    DataManager dm;
    auto time_frame = std::make_shared<TimeFrame>(std::vector<int>{0, 1, 2, 3, 4});
    dm.setTime(TimeKey("default"), time_frame);

    // 5 rows x 2 cols, dense time storage
    std::vector<float> data = {10.0f, 11.0f, 20.0f, 21.0f, 30.0f, 31.0f, 40.0f, 41.0f, 50.0f, 51.0f};
    auto ts = TimeIndexStorageFactory::createDenseFromZero(5);
    auto tensor = std::make_shared<TensorData>(
            TensorData::createTimeSeries2D(data, 5, 2, ts, time_frame, {"a", "b"}));
    tensor->setTimeFrame(time_frame);
    dm.setData("input_tensor", tensor, TimeKey("default"));

    std::filesystem::path test_dir =
            std::filesystem::temp_directory_path() / "tensor_temporal_neighbors_v2_test";
    std::filesystem::create_directories(test_dir);

    SECTION("Execute pipeline - NaN boundary") {
        char const * json_config = R"([
        {
            "transformations": {
                "metadata": {
                    "name": "Temporal Neighbors Pipeline",
                    "version": "2.0"
                },
                "steps": [
                    {
                        "step_id": "1",
                        "transform_name": "TensorTemporalNeighbors",
                        "input_key": "input_tensor",
                        "output_key": "augmented_tensor",
                        "parameters": {
                            "lag_range": 1,
                            "lag_step": 1,
                            "lead_range": 1,
                            "lead_step": 1,
                            "boundary_policy": "NaN",
                            "include_original": true
                        }
                    }
                ]
            }
        }
        ])";

        std::filesystem::path json_filepath = test_dir / "temporal_neighbors_nan.json";
        {
            std::ofstream f(json_filepath);
            REQUIRE(f.is_open());
            f << json_config;
        }

        auto info_list = load_data_from_json_config_v2(&dm, json_filepath.string());

        auto result = dm.getData<TensorData>("augmented_tensor");
        REQUIRE(result != nullptr);
        CHECK(result->numRows() == 5);
        // 2 original + 2 offsets x 2 cols = 6
        CHECK(result->numColumns() == 6);

        auto flat = result->materializeFlat();
        // Row 0: lag-1 should be NaN
        CHECK(std::isnan(flat[0 * 6 + 2]));
        CHECK(std::isnan(flat[0 * 6 + 3]));
        // Row 0: lag+1 should be row 1 values
        CHECK(flat[0 * 6 + 4] == 20.0f);
        CHECK(flat[0 * 6 + 5] == 21.0f);
    }

    SECTION("Execute pipeline - Drop boundary") {
        char const * json_config = R"([
        {
            "transformations": {
                "metadata": {
                    "name": "Temporal Neighbors Drop Pipeline",
                    "version": "2.0"
                },
                "steps": [
                    {
                        "step_id": "1",
                        "transform_name": "TensorTemporalNeighbors",
                        "input_key": "input_tensor",
                        "output_key": "augmented_tensor_drop",
                        "parameters": {
                            "lag_range": 1,
                            "lag_step": 1,
                            "lead_range": 1,
                            "lead_step": 1,
                            "boundary_policy": "Drop",
                            "include_original": true
                        }
                    }
                ]
            }
        }
        ])";

        std::filesystem::path json_filepath = test_dir / "temporal_neighbors_drop.json";
        {
            std::ofstream f(json_filepath);
            REQUIRE(f.is_open());
            f << json_config;
        }

        auto info_list = load_data_from_json_config_v2(&dm, json_filepath.string());

        auto result = dm.getData<TensorData>("augmented_tensor_drop");
        REQUIRE(result != nullptr);
        // Rows 0 and 4 dropped -> 3 surviving rows
        CHECK(result->numRows() == 3);
        CHECK(result->numColumns() == 6);
    }

    // Cleanup
    try {
        std::filesystem::remove_all(test_dir);
    } catch (...) {
    }
}
