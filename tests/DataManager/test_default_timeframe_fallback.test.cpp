/**
 * @file test_default_timeframe_fallback.test.cpp
 * @brief Tests for ensureDefaultTimeFrameFallback and JSON end-of-load integration
 */

#include <catch2/catch_test_macros.hpp>

#include "AnalogTimeSeries/Analog_Time_Series.hpp"
#include "DataManager.hpp"
#include "Media/Media_Data.hpp"
#include "TimeFrame/StrongTimeTypes.hpp"
#include "TimeFrame/TimeFrame.hpp"

#include <nlohmann/json.hpp>

#include <filesystem>
#include <numeric>
#include <string>

using json = nlohmann::json;

namespace {

std::string testDataBasePath() {
    auto const candidate = std::filesystem::current_path() / "data";
    if (std::filesystem::exists(candidate)) {
        return candidate.string();
    }
    return std::filesystem::current_path().string();
}

}// namespace

TEST_CASE("ensureDefaultTimeFrameFallback - no-op when time already populated",
          "[DataManager][timeframe][default_time_fallback]") {
    DataManager dm;
    std::vector<int> times(5);
    std::iota(times.begin(), times.end(), 0);
    REQUIRE(dm.setTime(TimeKey("time"), std::make_shared<TimeFrame>(times), true));

    REQUIRE_FALSE(ensureDefaultTimeFrameFallback(dm));
    REQUIRE(dm.getTime(TimeKey("time"))->getTotalFrameCount() == 5);
}

TEST_CASE("ensureDefaultTimeFrameFallback - no-op when no media with frames",
          "[DataManager][timeframe][default_time_fallback]") {
    DataManager dm;
    REQUIRE(dm.getTime(TimeKey("time"))->getTotalFrameCount() == 0);
    REQUIRE_FALSE(ensureDefaultTimeFrameFallback(dm));
    REQUIRE(dm.getTime(TimeKey("time"))->getTotalFrameCount() == 0);
}

TEST_CASE("load_data_from_json_config - points only leaves default time empty",
          "[DataManager][timeframe][default_time_fallback][json]") {
    DataManager dm;
    json const config = json::parse(R"([
        {
            "filepath": "Points/dlc_test.csv",
            "data_type": "points",
            "name": "dlc",
            "format": "dlc_csv",
            "likelihood_threshold": 0.9
        }
    ])");

    load_data_from_json_config(&dm, config, testDataBasePath());

    REQUIRE(dm.getTime(TimeKey("time"))->getTotalFrameCount() == 0);
    REQUIRE(!dm.getKeys<PointData>().empty());
}

#ifdef ENABLE_FFMPEG

TEST_CASE("load_data_from_json_config - video populates default time TimeFrame",
          "[DataManager][timeframe][default_time_fallback][json][video]") {
    DataManager dm;
    json const config = json::parse(R"([
        {
            "filepath": "Media/test_each_frame_number.mp4",
            "data_type": "video",
            "name": "media"
        }
    ])");

    load_data_from_json_config(&dm, config, testDataBasePath());

    auto const media = dm.getData<MediaData>("media");
    REQUIRE(media);
    REQUIRE(media->getTotalFrameCount() > 0);
    REQUIRE(dm.getTime(TimeKey("time"))->getTotalFrameCount() == media->getTotalFrameCount());
}

TEST_CASE("load_data_from_json_config - video non-media key populates default time",
          "[DataManager][timeframe][default_time_fallback][json][video]") {
    DataManager dm;
    json const config = json::parse(R"([
        {
            "filepath": "Media/test_each_frame_number.mp4",
            "data_type": "video",
            "name": "top_cam"
        }
    ])");

    load_data_from_json_config(&dm, config, testDataBasePath());

    auto const top_cam = dm.getData<MediaData>("top_cam");
    REQUIRE(top_cam);
    REQUIRE(top_cam->getTotalFrameCount() > 0);
    REQUIRE(dm.getTime(TimeKey("time"))->getTotalFrameCount() == top_cam->getTotalFrameCount());
}

TEST_CASE("load_data_from_json_config - explicit time entry prevents fallback overwrite",
          "[DataManager][timeframe][default_time_fallback][json][video]") {
    DataManager dm;

    std::vector<float> const values(10, 1.0f);
    std::vector<TimeFrameIndex> time_indices;
    time_indices.reserve(10);
    for (int i = 0; i < 10; ++i) {
        time_indices.emplace_back(i);
    }
    auto analog = std::make_shared<AnalogTimeSeries>(values, time_indices);
    dm.setData<AnalogTimeSeries>("src", analog, TimeKey("time"));

    json const config = json::parse(R"([
        {
            "format": "max_value",
            "data_type": "time",
            "name": "time",
            "source_data": "src"
        },
        {
            "filepath": "Media/test_each_frame_number.mp4",
            "data_type": "video",
            "name": "media"
        }
    ])");

    load_data_from_json_config(&dm, config, testDataBasePath());

    REQUIRE(dm.getTime(TimeKey("time"))->getTotalFrameCount() == 10);

    auto const media = dm.getData<MediaData>("media");
    REQUIRE(media);
    REQUIRE(media->getTotalFrameCount() > 0);
    REQUIRE(dm.getTime(TimeKey("time"))->getTotalFrameCount() != media->getTotalFrameCount());
}

#endif// ENABLE_FFMPEG

TEST_CASE("load_data_from_json_config - clocks array registers forward-declared TimeFrames",
          "[DataManager][timeframe][json][clocks]") {
    DataManager dm;

    std::vector<float> const values(5, 1.0f);
    std::vector<TimeFrameIndex> time_indices;
    time_indices.reserve(5);
    for (int i = 0; i < 5; ++i) {
        time_indices.emplace_back(i);
    }
    auto analog = std::make_shared<AnalogTimeSeries>(values, time_indices);
    dm.setData<AnalogTimeSeries>("src", analog, TimeKey("time"));

    json const config = json::parse(R"({
        "clocks": ["master"],
        "data": [
            {
                "format": "max_value",
                "data_type": "time",
                "name": "master",
                "source_data": "src"
            }
        ]
    })");

    load_data_from_json_config(&dm, config, testDataBasePath());

    auto const master = dm.getTime(TimeKey("master"));
    REQUIRE(master);
    REQUIRE(master->getTotalFrameCount() == 5);
}

TEST_CASE("load_data_from_json_config - deferred clock binding allows data before clock population",
          "[DataManager][timeframe][json][clocks]") {
    DataManager dm;

    std::vector<float> const values(5, 1.0f);
    std::vector<TimeFrameIndex> time_indices;
    time_indices.reserve(5);
    for (int i = 0; i < 5; ++i) {
        time_indices.emplace_back(i);
    }
    auto analog = std::make_shared<AnalogTimeSeries>(values, time_indices);
    dm.setData<AnalogTimeSeries>("src", analog, TimeKey("time"));

    json const config = json::parse(R"({
        "clocks": ["master"],
        "data": [
            {
                "filepath": "Analog/single_column.csv",
                "data_type": "analog",
                "name": "breath",
                "format": "csv",
                "single_column_format": true,
                "has_header": false,
                "clock": "master"
            },
            {
                "format": "max_value",
                "data_type": "time",
                "name": "master",
                "source_data": "src"
            }
        ]
    })");

    load_data_from_json_config(&dm, config, testDataBasePath());

    REQUIRE(dm.getTime(TimeKey("master"))->getTotalFrameCount() == 5);
    REQUIRE(dm.getTimeKey("breath") == TimeKey("master"));
    REQUIRE(dm.getData<AnalogTimeSeries>("breath") != nullptr);
}

TEST_CASE("load_data_from_json_config - declared clock assigns data at registration time",
          "[DataManager][timeframe][json][clocks]") {
    DataManager dm;

    json const config = json::parse(R"({
        "clocks": ["master"],
        "data": [
            {
                "filepath": "Analog/single_column.csv",
                "data_type": "analog",
                "name": "breath",
                "format": "csv",
                "single_column_format": true,
                "has_header": false,
                "clock": "master"
            }
        ]
    })");

    load_data_from_json_config(&dm, config, testDataBasePath());

    REQUIRE(dm.getTimeKey("breath") == TimeKey("master"));
    REQUIRE(dm.getData<AnalogTimeSeries>("breath") != nullptr);
}

TEST_CASE("load_data_from_json_config - time entries before data resolve clock without clocks array",
          "[DataManager][timeframe][json][clocks]") {
    DataManager dm;

    std::vector<float> const values(5, 1.0f);
    std::vector<TimeFrameIndex> time_indices;
    time_indices.reserve(5);
    for (int i = 0; i < 5; ++i) {
        time_indices.emplace_back(i);
    }
    auto analog = std::make_shared<AnalogTimeSeries>(values, time_indices);
    dm.setData<AnalogTimeSeries>("src", analog, TimeKey("time"));

    json const config = json::parse(R"({
        "data": [
            {
                "format": "max_value",
                "data_type": "time",
                "name": "master",
                "source_data": "src"
            },
            {
                "filepath": "Analog/single_column.csv",
                "data_type": "analog",
                "name": "breath",
                "format": "csv",
                "single_column_format": true,
                "has_header": false,
                "clock": "master"
            }
        ]
    })");

    load_data_from_json_config(&dm, config, testDataBasePath());

    REQUIRE(dm.getTime(TimeKey("master"))->getTotalFrameCount() == 5);
    REQUIRE(dm.getTimeKey("breath") == TimeKey("master"));
    REQUIRE(dm.getData<AnalogTimeSeries>("breath") != nullptr);
}

TEST_CASE("load_data_from_json_config - undeclared clock population fails validation",
          "[DataManager][timeframe][json][clocks]") {
    DataManager dm;

    json const config = json::parse(R"({
        "clocks": ["master"],
        "data": [
            {
                "filepath": "Analog/single_column.csv",
                "data_type": "analog",
                "name": "breath",
                "format": "csv",
                "single_column_format": true,
                "has_header": false,
                "clock": "master"
            }
        ]
    })");

    load_data_from_json_config(&dm, config, testDataBasePath());

    REQUIRE(dm.getTime(TimeKey("master"))->getTotalFrameCount() == 0);
    REQUIRE(dm.getTimeKey("breath") == TimeKey("master"));
    REQUIRE(dm.getData<AnalogTimeSeries>("breath") != nullptr);
}
