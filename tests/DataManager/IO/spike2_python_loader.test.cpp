/**
 * @file spike2_python_loader.test.cpp
 * @brief Tests for Spike2PythonFormatLoader payload conversion.
 */

#include "IO/formats/Spike2/Spike2PythonFormatLoader.hpp"

#include "AnalogTimeSeries/Analog_Time_Series.hpp"
#include "DigitalTimeSeries/Digital_Event_Series.hpp"
#include "DigitalTimeSeries/Digital_Interval_Series.hpp"
#include "PythonEngine.hpp"

#include <catch2/catch_test_macros.hpp>

#include <filesystem>
#include <memory>
#include <ranges>
#include <variant>

namespace {

PythonEngine & engine() {
    static PythonEngine eng;
    return eng;
}

std::string helperPath() {
    return (std::filesystem::current_path() / "resources" / "python").string();
}

};// namespace

TEST_CASE("Spike2PythonFormatLoader converts fake analog payload", "[io][spike2]") {
    Spike2PythonFormatLoader loader(engine());
    auto config = nlohmann::json{
            {"python_helper_path", helperPath()},
            {"fake_payload", {
                                     {"analog", {{{"name", "Breath_raw"}, {"times", {10, 12, 14}}, {"values", {1.5, 2.5, 3.5}}}}},
                             }}};

    auto result = loader.loadBatch("fake.smrx", DM_DataType::Analog, config);

    INFO(result.error_message);
    REQUIRE(result.success);
    REQUIRE(result.results.size() == 1);
    REQUIRE(result.results.front().name == "Breath_raw");

    auto analog = std::get<std::shared_ptr<AnalogTimeSeries>>(result.results.front().data);
    REQUIRE(analog->getNumSamples() == 3);
    auto samples = analog->view();
    REQUIRE((*std::ranges::next(samples.begin(), 0)).time() == TimeFrameIndex{10});
    REQUIRE((*std::ranges::next(samples.begin(), 2)).time() == TimeFrameIndex{14});
}

TEST_CASE("Spike2PythonFormatLoader converts fake digital event payload", "[io][spike2]") {
    Spike2PythonFormatLoader loader(engine());
    auto config = nlohmann::json{
            {"python_helper_path", helperPath()},
            {"fake_payload", {
                                     {"events", {{{"name", "Frame_start"}, {"times", {3, 8, 13}}}}},
                             }}};

    auto result = loader.loadBatch("fake.smrx", DM_DataType::DigitalEvent, config);

    INFO(result.error_message);
    REQUIRE(result.success);
    REQUIRE(result.results.size() == 1);
    REQUIRE(result.results.front().name == "Frame_start");

    auto events = std::get<std::shared_ptr<DigitalEventSeries>>(result.results.front().data);
    REQUIRE(events->size() == 3);
    REQUIRE(events->getStoredEvent(0) == TimeFrameIndex{3});
    REQUIRE(events->getStoredEvent(2) == TimeFrameIndex{13});
}

TEST_CASE("Spike2PythonFormatLoader converts fake interval payload", "[io][spike2]") {
    Spike2PythonFormatLoader loader(engine());
    auto config = nlohmann::json{
            {"python_helper_path", helperPath()},
            {"fake_payload", {
                                     {"intervals", {{{"name", "US_start_stop"}, {"starts", {20, 40}}, {"ends", {25, 45}}}}},
                             }}};

    auto result = loader.loadBatch("fake.smrx", DM_DataType::DigitalInterval, config);

    INFO(result.error_message);
    REQUIRE(result.success);
    REQUIRE(result.results.size() == 1);
    REQUIRE(result.results.front().name == "US_start_stop");

    auto intervals = std::get<std::shared_ptr<DigitalIntervalSeries>>(result.results.front().data);
    REQUIRE(intervals->size() == 2);
    REQUIRE(intervals->getStoredInterval(0).start == TimeFrameIndex{20});
    REQUIRE(intervals->getStoredInterval(1).end == TimeFrameIndex{45});
}
