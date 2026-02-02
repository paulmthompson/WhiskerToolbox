#ifndef DIGITAL_EVENT_CSV_LOADING_SCENARIOS_HPP
#define DIGITAL_EVENT_CSV_LOADING_SCENARIOS_HPP

#include "fixtures/builders/DigitalTimeSeriesBuilder.hpp"
#include "DigitalTimeSeries/Digital_Event_Series.hpp"
#include "TimeFrame/StrongTimeTypes.hpp"

#include <filesystem>
#include <fstream>
#include <memory>
#include <string>
#include <vector>

namespace digital_event_scenarios {

/**
 * @brief Helper to write DigitalEventSeries data to CSV with single event column
 * 
 * Writes single-column CSV with optional header. Column contains event timestamps.
 * 
 * @param events The DigitalEventSeries to write
 * @param filepath Output file path
 * @param delimiter Column delimiter (default ",")
 * @param write_header Whether to write header row
 * @param header_text Header text if write_header is true
 * @return true if write succeeded
 */
inline bool writeCSVSingleColumn(DigitalEventSeries const* events,
                                 std::string const& filepath,
                                 std::string const& delimiter = ",",
                                 bool write_header = true,
                                 std::string const& header_text = "Event") {
    std::ofstream file(filepath);
    if (!file.is_open()) {
        return false;
    }
    
    // Write header if requested
    if (write_header) {
        file << header_text << "\n";
    }
    
    // Write data rows
    for (auto const& event : events->view()) {
        file << event.time().getValue() << "\n";
    }
    
    return file.good();
}

/**
 * @brief Helper to write DigitalEventSeries with custom delimiter
 * 
 * @param events The DigitalEventSeries to write
 * @param filepath Output file path
 * @param delimiter Column delimiter (tab, semicolon, etc.)
 * @return true if write succeeded
 */
inline bool writeCSVWithDelimiter(DigitalEventSeries const* events,
                                  std::string const& filepath,
                                  std::string const& delimiter) {
    return writeCSVSingleColumn(events, filepath, delimiter, true, "Event");
}

/**
 * @brief Helper to write DigitalEventSeries without header
 * 
 * @param events The DigitalEventSeries to write
 * @param filepath Output file path
 * @param delimiter Column delimiter
 * @return true if write succeeded
 */
inline bool writeCSVNoHeader(DigitalEventSeries const* events,
                             std::string const& filepath,
                             std::string const& delimiter = ",") {
    return writeCSVSingleColumn(events, filepath, delimiter, false, "");
}

/**
 * @brief Helper to write DigitalEventSeries with event column at specific index
 * 
 * Writes CSV with padding columns before the event column.
 * Useful for testing event_column configuration.
 * 
 * @param events The DigitalEventSeries to write
 * @param filepath Output file path
 * @param event_column_index Which column (0-based) to put events in
 * @param delimiter Column delimiter
 * @param write_header Whether to write header row
 * @return true if write succeeded
 */
inline bool writeCSVWithEventColumn(DigitalEventSeries const* events,
                                    std::string const& filepath,
                                    int event_column_index,
                                    std::string const& delimiter = ",",
                                    bool write_header = true) {
    std::ofstream file(filepath);
    if (!file.is_open()) {
        return false;
    }
    
    // Write header if requested
    if (write_header) {
        for (int i = 0; i < event_column_index; ++i) {
            file << "Col" << i << delimiter;
        }
        file << "Event";
        for (int i = event_column_index + 1; i < event_column_index + 2; ++i) {
            file << delimiter << "Col" << i;
        }
        file << "\n";
    }
    
    // Write data rows
    int row_id = 0;
    for (auto const& event : events->view()) {
        for (int i = 0; i < event_column_index; ++i) {
            file << row_id << delimiter;
        }
        file << event.time().getValue();
        for (int i = event_column_index + 1; i < event_column_index + 2; ++i) {
            file << delimiter << row_id;
        }
        file << "\n";
        ++row_id;
    }
    
    return file.good();
}

/**
 * @brief Helper to write multiple DigitalEventSeries with identifier column
 * 
 * Writes CSV with event column and identifier column for multi-series data.
 * Useful for testing batch loading with identifier_column configuration.
 * 
 * @param series_map Map of identifier to DigitalEventSeries
 * @param filepath Output file path
 * @param delimiter Column delimiter
 * @param write_header Whether to write header row
 * @return true if write succeeded
 */
inline bool writeCSVWithIdentifiers(
        std::vector<std::pair<std::string, std::shared_ptr<DigitalEventSeries>>> const& series_list,
        std::string const& filepath,
        std::string const& delimiter = ",",
        bool write_header = true) {
    std::ofstream file(filepath);
    if (!file.is_open()) {
        return false;
    }
    
    // Write header if requested
    if (write_header) {
        file << "Event" << delimiter << "Identifier" << "\n";
    }
    
    // Write data rows for all series
    for (auto const& [identifier, events] : series_list) {
        for (auto const& event : events->view()) {
            file << event.time().getValue() << delimiter << identifier << "\n";
        }
    }
    
    return file.good();
}

//=============================================================================
// Pre-configured test event series for CSV loading tests
//=============================================================================

/**
 * @brief Simple event series with 5 events
 * 
 * Creates events at: 10, 50, 100, 200, 300
 */
inline std::shared_ptr<DigitalEventSeries> simple_events() {
    return DigitalEventSeriesBuilder()
        .withEvents({10, 50, 100, 200, 300})
        .build();
}

/**
 * @brief Single event for minimal test case
 * 
 * Creates one event at: 100
 */
inline std::shared_ptr<DigitalEventSeries> single_event() {
    return DigitalEventSeriesBuilder()
        .withEvents({100})
        .build();
}

/**
 * @brief Many events using regular interval pattern
 * 
 * Creates 10 events at interval of 10 starting from 0
 * Pattern: 0, 10, 20, 30, ..., 90
 */
inline std::shared_ptr<DigitalEventSeries> regular_pattern_events() {
    return DigitalEventSeriesBuilder()
        .withInterval(0, 90, 10)
        .build();
}

/**
 * @brief Events with large time values
 * 
 * Tests handling of large 64-bit time values
 */
inline std::shared_ptr<DigitalEventSeries> large_time_events() {
    return DigitalEventSeriesBuilder()
        .withEvents({1000000, 2000000, 5000000, 10000000})
        .build();
}

/**
 * @brief Dense events (many events close together)
 * 
 * Creates events at: 0, 1, 2, 3, 4, 5
 */
inline std::shared_ptr<DigitalEventSeries> dense_events() {
    return DigitalEventSeriesBuilder()
        .withInterval(0, 5, 1)
        .build();
}

/**
 * @brief Sparse events (widely spaced)
 * 
 * Creates events at: 0, 1000, 5000, 10000, 50000
 */
inline std::shared_ptr<DigitalEventSeries> sparse_events() {
    return DigitalEventSeriesBuilder()
        .withEvents({0, 1000, 5000, 10000, 50000})
        .build();
}

/**
 * @brief Many events for stress testing
 * 
 * Creates 100 events at interval of 10
 */
inline std::shared_ptr<DigitalEventSeries> many_events() {
    return DigitalEventSeriesBuilder()
        .withInterval(0, 990, 10)
        .build();
}

/**
 * @brief Events starting at zero
 * 
 * Tests edge case of event at time 0
 */
inline std::shared_ptr<DigitalEventSeries> events_starting_at_zero() {
    return DigitalEventSeriesBuilder()
        .withEvents({0, 10, 20, 30})
        .build();
}

/**
 * @brief Create multiple named event series for batch loading tests
 * 
 * Returns a vector of (identifier, DigitalEventSeries) pairs
 */
inline std::vector<std::pair<std::string, std::shared_ptr<DigitalEventSeries>>> 
multi_series_events() {
    return {
        {"seriesA", DigitalEventSeriesBuilder().withEvents({10, 20, 30}).build()},
        {"seriesB", DigitalEventSeriesBuilder().withEvents({15, 25, 35, 45}).build()},
        {"seriesC", DigitalEventSeriesBuilder().withEvents({5, 50}).build()}
    };
}

/**
 * @brief Create two event series for simple batch loading tests
 */
inline std::vector<std::pair<std::string, std::shared_ptr<DigitalEventSeries>>> 
two_series_events() {
    return {
        {"type1", DigitalEventSeriesBuilder().withEvents({100, 200, 300}).build()},
        {"type2", DigitalEventSeriesBuilder().withEvents({150, 250, 350, 450}).build()}
    };
}

} // namespace digital_event_scenarios

#endif // DIGITAL_EVENT_CSV_LOADING_SCENARIOS_HPP
