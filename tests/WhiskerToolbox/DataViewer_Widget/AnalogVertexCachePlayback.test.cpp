/**
 * @file AnalogVertexCachePlayback.test.cpp
 * @brief Regression tests for AnalogVertexCache + cached analog batching during view slides ("play").
 */

#include "Rendering/SceneBuildingHelpers.hpp"

#include "AnalogTimeSeries/Analog_Time_Series.hpp"
#include "TimeFrame/TimeFrame.hpp"

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

#include <glm/glm.hpp>

#include <cstdint>
#include <memory>
#include <vector>

using Catch::Matchers::WithinAbs;
using namespace DataViewer;

TEST_CASE("AnalogVertexCache playback: sliding window matches reference batch", "[AnalogVertexCache][DataViewer]") {
    int constexpr kFrames = 500;
    std::vector<int> times(static_cast<size_t>(kFrames));
    for (int i = 0; i < kFrames; ++i) {
        times[static_cast<size_t>(i)] = i;
    }
    auto master = std::make_shared<TimeFrame>(times);

    std::vector<float> values(static_cast<size_t>(kFrames));
    for (int i = 0; i < kFrames; ++i) {
        values[static_cast<size_t>(i)] = ((i % 40) < 20) ? 1.0f : -1.0f;
    }
    auto series = std::make_shared<AnalogTimeSeries>(std::move(values), static_cast<size_t>(kFrames));
    series->setTimeFrame(master);

    AnalogVertexCache cache;
    glm::mat4 const model{1.0f};

    DataViewerHelpers::AnalogBatchParams first{};
    first.start_time = TimeFrameIndex{0};
    first.end_time = TimeFrameIndex{100};
    first.x_origin_master_absolute_time = static_cast<int64_t>(master->getTimeAtIndex(first.start_time));
    first.detect_gaps = false;
    first.min_max_decimation_bucket_count = 0;

    (void) DataViewerHelpers::buildAnalogSeriesBatchCached(
            *series, master, first, model, cache);

    DataViewerHelpers::AnalogBatchParams second{};
    second.start_time = TimeFrameIndex{25};
    second.end_time = TimeFrameIndex{125};
    second.x_origin_master_absolute_time = static_cast<int64_t>(master->getTimeAtIndex(second.start_time));
    second.detect_gaps = false;
    second.min_max_decimation_bucket_count = 0;

    auto const cached_batch =
            DataViewerHelpers::buildAnalogSeriesBatchCached(*series, master, second, model, cache);

    DataViewerHelpers::AnalogBatchParams ref_params = second;
    auto const ref_batch =
            DataViewerHelpers::buildAnalogSeriesBatchSimplified(*series, master, ref_params, model);

    REQUIRE_FALSE(cached_batch.vertices.empty());
    REQUIRE(cached_batch.vertices.size() == ref_batch.vertices.size());

    for (size_t i = 0; i < cached_batch.vertices.size(); ++i) {
        CHECK_THAT(cached_batch.vertices[i], WithinAbs(ref_batch.vertices[i], 1e-3f));
    }
}

TEST_CASE("AnalogVertexCache playback: large absolute times preserve visible sample spacing", "[AnalogVertexCache][DataViewer]") {
    int constexpr kFrames = 512;
    int constexpr kLargeTimeBase = 29026347;

    std::vector<int> times(static_cast<size_t>(kFrames));
    for (int i = 0; i < kFrames; ++i) {
        times[static_cast<size_t>(i)] = kLargeTimeBase + i;
    }
    auto master = std::make_shared<TimeFrame>(times);

    std::vector<float> values(static_cast<size_t>(kFrames));
    for (int i = 0; i < kFrames; ++i) {
        values[static_cast<size_t>(i)] = (i % 2 == 0) ? 1.0f : -1.0f;
    }
    auto series = std::make_shared<AnalogTimeSeries>(std::move(values), static_cast<size_t>(kFrames));
    series->setTimeFrame(master);

    AnalogVertexCache cache;
    glm::mat4 const model{1.0f};

    DataViewerHelpers::AnalogBatchParams params{};
    params.start_time = TimeFrameIndex{128};
    params.end_time = TimeFrameIndex{228};
    params.x_origin_master_absolute_time = static_cast<int64_t>(master->getTimeAtIndex(params.start_time));
    params.detect_gaps = false;
    params.min_max_decimation_bucket_count = 0;

    auto const cached_batch =
            DataViewerHelpers::buildAnalogSeriesBatchCached(*series, master, params, model, cache);
    auto const ref_batch =
            DataViewerHelpers::buildAnalogSeriesBatchSimplified(*series, master, params, model);

    REQUIRE_FALSE(cached_batch.vertices.empty());
    REQUIRE(cached_batch.vertices.size() == ref_batch.vertices.size());

    for (size_t i = 0; i < cached_batch.vertices.size(); i += 2) {
        CHECK_THAT(cached_batch.vertices[i], WithinAbs(ref_batch.vertices[i], 1e-3f));
    }
}
