#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>

#include "fixtures/data_manager_test_fixtures.hpp"

TEST_CASE_METHOD(DataManagerTestFixture, "DataManagerTestFixture - Basic Data Population", "[DataManager][fixtures]") {
    // Test that the DataManager was properly initialized
    auto& dm = getDataManager();
    REQUIRE(&dm != nullptr);
    
    // Test that all expected data types are present
    SECTION("PointData population") {
        auto point_data = dm.getData<PointData>("test_points");
        REQUIRE(point_data != nullptr);
        REQUIRE(point_data->getAllEntries().size() > 0);
    }
    
    SECTION("LineData population") {
        auto line_data = dm.getData<LineData>("test_lines");
        REQUIRE(line_data != nullptr);
        REQUIRE(line_data->getAllEntries().size() > 0);
    }
    
    SECTION("MaskData population") {
        auto mask_data = dm.getData<MaskData>("test_masks");
        REQUIRE(mask_data != nullptr);
        REQUIRE(mask_data->getAllAsRange().size() > 0);
    }
    
    SECTION("AnalogTimeSeries population") {
        auto analog_data = dm.getData<AnalogTimeSeries>("test_analog");
        REQUIRE(analog_data != nullptr);
        REQUIRE(analog_data->getNumSamples() > 0);
        
        auto analog_data_2 = dm.getData<AnalogTimeSeries>("test_analog_2");
        REQUIRE(analog_data_2 != nullptr);
        REQUIRE(analog_data_2->getNumSamples() > 0);
    }
    
    SECTION("DigitalEventSeries population") {
        auto event_data = dm.getData<DigitalEventSeries>("test_events");
        REQUIRE(event_data != nullptr);
        REQUIRE(event_data->size() > 0);
        
        auto event_data_2 = dm.getData<DigitalEventSeries>("test_events_2");
        REQUIRE(event_data_2 != nullptr);
        REQUIRE(event_data_2->size() > 0);
    }
    
    SECTION("DigitalIntervalSeries population") {
        auto interval_data = dm.getData<DigitalIntervalSeries>("test_intervals");
        REQUIRE(interval_data != nullptr);
        REQUIRE(interval_data->size() > 0);
        
        auto interval_data_2 = dm.getData<DigitalIntervalSeries>("test_intervals_2");
        REQUIRE(interval_data_2 != nullptr);
        REQUIRE(interval_data_2->size() > 0);
    }
}

TEST_CASE_METHOD(DataManagerRandomTestFixture, "DataManagerRandomTestFixture - Random Data Generation", "[DataManager][fixtures][random]") {
    // Test that the DataManager was properly initialized with random data
    auto& dm = getDataManager();
    REQUIRE(&dm != nullptr);
    
    // Test random data generation
    SECTION("Random PointData") {
        auto random_points = dm.getData<PointData>("random_points");
        REQUIRE(random_points != nullptr);
        REQUIRE(random_points->getAllEntries().size() > 0);
    }
    
    SECTION("Random LineData") {
        auto random_lines = dm.getData<LineData>("random_lines");
        REQUIRE(random_lines != nullptr);
        REQUIRE(random_lines->getAllEntries().size() > 0);
    }
    
    SECTION("Random AnalogTimeSeries") {
        auto random_analog = dm.getData<AnalogTimeSeries>("random_analog");
        REQUIRE(random_analog != nullptr);
        REQUIRE(random_analog->getNumSamples() > 0);
    }
    
    SECTION("Random DigitalEventSeries") {
        auto random_events = dm.getData<DigitalEventSeries>("random_events");
        REQUIRE(random_events != nullptr);
        REQUIRE(random_events->size() > 0);
    }
    
    SECTION("Random DigitalIntervalSeries") {
        auto random_intervals = dm.getData<DigitalIntervalSeries>("random_intervals");
        REQUIRE(random_intervals != nullptr);
        REQUIRE(random_intervals->size() > 0);
    }
    
    SECTION("Random engine access") {
        auto& random_engine = getRandomEngine();
        REQUIRE(&random_engine != nullptr);
        
        // Test that we can generate additional random data
        std::uniform_real_distribution<float> dist(0.0f, 1.0f);
        float random_value = dist(random_engine);
        REQUIRE(random_value >= 0.0f);
        REQUIRE(random_value <= 1.0f);
    }
} 