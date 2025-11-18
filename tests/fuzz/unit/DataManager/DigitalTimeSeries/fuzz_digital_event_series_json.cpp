/**
 * @file fuzz_digital_event_series_json.cpp
 * @brief Fuzz tests for Digital Event Series JSON loading
 * 
 * Tests the robustness of JSON parsing for digital event series data.
 * Focuses on:
 * - Corrupted JSON structures
 * - Invalid parameter values
 * - Missing required fields
 * - Nonsensical parameter combinations
 */

#include "fuzztest/fuzztest.h"
#include "gtest/gtest.h"

#include "DigitalTimeSeries/IO/JSON/Digital_Event_Series_JSON.hpp"
#include "DigitalTimeSeries/Digital_Event_Series.hpp"
#include "nlohmann/json.hpp"

#include <filesystem>
#include <fstream>
#include <string>
#include <vector>

namespace {

/**
 * @brief Test that JSON parsing doesn't crash with arbitrary JSON input
 * 
 * This fuzz test verifies that the loader handles corrupted or malformed JSON
 * gracefully without crashing, even with invalid data.
 */
void FuzzJsonStructure(const std::string& json_str) {
    try {
        auto json_obj = nlohmann::json::parse(json_str);
        
        // Create a temporary file (we don't actually need valid data for this test)
        std::string temp_file = std::tmpnam(nullptr);
        
        // Attempt to load - should not crash
        auto result = load_into_DigitalEventSeries(temp_file, json_obj);
        
        // Clean up
        std::filesystem::remove(temp_file);
        
        // We don't care if it succeeds or fails, just that it doesn't crash
        // Result may be empty or populated depending on input validity
    } catch (const nlohmann::json::parse_error&) {
        // JSON parsing failures are expected and acceptable
    } catch (const std::exception&) {
        // Other exceptions are caught but we allow them
        // The function should handle errors gracefully
    }
}
FUZZ_TEST(DigitalEventSeriesJsonFuzz, FuzzJsonStructure);

/**
 * @brief Test with structured but potentially invalid JSON objects
 * 
 * This test generates JSON objects with specific fields that might be present
 * in a real configuration but with fuzzy values.
 */
void FuzzValidJsonStructure(
    const std::string& format,
    int channel,
    const std::string& transition,
    int header_size,
    int channel_count,
    float scale,
    bool scale_divide) {
    
    try {
        nlohmann::json json_obj;
        json_obj["format"] = format;
        json_obj["channel"] = channel;
        json_obj["transition"] = transition;
        json_obj["header_size"] = header_size;
        json_obj["channel_count"] = channel_count;
        json_obj["scale"] = scale;
        json_obj["scale_divide"] = scale_divide;
        
        // Create a temporary file
        std::string temp_file = std::tmpnam(nullptr);
        
        // Attempt to load
        auto result = load_into_DigitalEventSeries(temp_file, json_obj);
        
        // Clean up
        std::filesystem::remove(temp_file);
        
        // Function should handle all inputs gracefully
    } catch (const std::exception&) {
        // Exceptions are acceptable for invalid configurations
    }
}
FUZZ_TEST(DigitalEventSeriesJsonFuzz, FuzzValidJsonStructure)
    .WithDomains(
        fuzztest::StringOf(fuzztest::InRange('a', 'z')).WithMinSize(1).WithMaxSize(20),
        fuzztest::InRange(-100, 100),
        fuzztest::OneOf(fuzztest::Just(std::string("rising")), 
                       fuzztest::Just(std::string("falling")), 
                       fuzztest::Arbitrary<std::string>()),
        fuzztest::InRange(-1000, 1000),
        fuzztest::InRange(-10, 100),
        fuzztest::InRange(-1000.0f, 1000.0f),
        fuzztest::Arbitrary<bool>()
    );

/**
 * @brief Test CSV format configuration
 */
void FuzzCsvJsonStructure(
    const std::string& delimiter,
    bool has_header,
    int event_column,
    int identifier_column,
    const std::string& name) {
    
    try {
        nlohmann::json json_obj;
        json_obj["format"] = "csv";
        json_obj["delimiter"] = delimiter;
        json_obj["has_header"] = has_header;
        json_obj["event_column"] = event_column;
        json_obj["identifier_column"] = identifier_column;
        json_obj["name"] = name;
        
        // Create a temporary CSV file with some data
        std::string temp_file = std::tmpnam(nullptr);
        std::ofstream file(temp_file);
        if (has_header) {
            file << "time" << delimiter << "label\n";
        }
        file << "100" << delimiter << "event1\n";
        file << "200" << delimiter << "event2\n";
        file.close();
        
        // Attempt to load
        auto result = load_into_DigitalEventSeries(temp_file, json_obj);
        
        // Clean up
        std::filesystem::remove(temp_file);
        
    } catch (const std::exception&) {
        // Exceptions are acceptable
    }
}
FUZZ_TEST(DigitalEventSeriesJsonFuzz, FuzzCsvJsonStructure)
    .WithDomains(
        fuzztest::OneOf(fuzztest::Just(std::string(",")), 
                       fuzztest::Just(std::string("\t")), 
                       fuzztest::Just(std::string(";")),
                       fuzztest::Arbitrary<std::string>().WithMaxSize(3)),
        fuzztest::Arbitrary<bool>(),
        fuzztest::InRange(-5, 10),
        fuzztest::InRange(-5, 10),
        fuzztest::StringOf(fuzztest::InRange('a', 'z')).WithMaxSize(30)
    );

/**
 * @brief Test the EventDataType string parsing
 */
void FuzzEventDataTypeString(const std::string& type_str) {
    // Should never crash regardless of input
    EventDataType result = stringToEventDataType(type_str);
    
    // Result should be one of the valid enum values
    EXPECT_TRUE(result == EventDataType::uint16 || 
                result == EventDataType::csv || 
                result == EventDataType::Unknown);
}
FUZZ_TEST(DigitalEventSeriesJsonFuzz, FuzzEventDataTypeString);

/**
 * @brief Test event scaling with various scale factors
 */
void FuzzEventScaling(
    const std::vector<int64_t>& event_values,
    float scale,
    bool scale_divide) {
    
    // Convert to TimeFrameIndex
    std::vector<TimeFrameIndex> events;
    for (auto val : event_values) {
        events.push_back(TimeFrameIndex(val));
    }
    
    try {
        // Should not crash with any scale value
        scale_events(events, scale, scale_divide);
        
        // Verify events container is still valid (no corruption)
        EXPECT_GE(events.size(), 0);
        
    } catch (const std::exception&) {
        // Division by zero or overflow are acceptable exceptions
    }
}
FUZZ_TEST(DigitalEventSeriesJsonFuzz, FuzzEventScaling)
    .WithDomains(
        fuzztest::VectorOf(fuzztest::InRange<int64_t>(-1000000, 1000000))
            .WithMaxSize(1000),
        fuzztest::InRange(-100.0f, 100.0f),
        fuzztest::Arbitrary<bool>()
    );

} // namespace
