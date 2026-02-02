#ifndef DIGITAL_INTERVAL_CSV_LOADING_SCENARIOS_HPP
#define DIGITAL_INTERVAL_CSV_LOADING_SCENARIOS_HPP

#include "fixtures/builders/DigitalTimeSeriesBuilder.hpp"
#include "DigitalTimeSeries/Digital_Interval_Series.hpp"
#include "TimeFrame/interval_data.hpp"

#include <filesystem>
#include <fstream>
#include <memory>
#include <string>
#include <vector>

namespace digital_interval_scenarios {

/**
 * @brief Helper to write DigitalIntervalSeries data to CSV with start and end columns
 * 
 * Writes two-column CSV with optional header. Start column contains
 * interval start times, End column contains interval end times.
 * 
 * @param intervals The DigitalIntervalSeries to write
 * @param filepath Output file path
 * @param delimiter Column delimiter (default ",")
 * @param write_header Whether to write header row
 * @param header_text Header text if write_header is true
 * @return true if write succeeded
 */
inline bool writeCSVTwoColumn(DigitalIntervalSeries const* intervals,
                              std::string const& filepath,
                              std::string const& delimiter = ",",
                              bool write_header = true,
                              std::string const& header_text = "Start,End") {
    std::ofstream file(filepath);
    if (!file.is_open()) {
        return false;
    }
    
    // Write header if requested
    if (write_header) {
        file << header_text << "\n";
    }
    
    // Write data rows
    for (auto const& interval : intervals->view()) {
        file << interval.value().start << delimiter << interval.value().end << "\n";
    }
    
    return file.good();
}

/**
 * @brief Helper to write DigitalIntervalSeries with reversed column order (end, start)
 * 
 * Writes CSV with end column first, then start column (reversed order).
 * Useful for testing flip_column_order configuration.
 * 
 * @param intervals The DigitalIntervalSeries to write
 * @param filepath Output file path
 * @param delimiter Column delimiter
 * @param write_header Whether to write header row
 * @return true if write succeeded
 */
inline bool writeCSVReversedColumns(DigitalIntervalSeries const* intervals,
                                    std::string const& filepath,
                                    std::string const& delimiter = ",",
                                    bool write_header = true) {
    std::ofstream file(filepath);
    if (!file.is_open()) {
        return false;
    }
    
    // Write header if requested (End,Start order)
    if (write_header) {
        file << "End" << delimiter << "Start" << "\n";
    }
    
    // Write data rows (end first, then start)
    for (auto const& interval : intervals->view()) {
        file << interval.value().end << delimiter << interval.value().start << "\n";
    }
    
    return file.good();
}

/**
 * @brief Helper to write DigitalIntervalSeries with custom delimiter
 * 
 * @param intervals The DigitalIntervalSeries to write
 * @param filepath Output file path
 * @param delimiter Column delimiter (tab, semicolon, etc.)
 * @return true if write succeeded
 */
inline bool writeCSVWithDelimiter(DigitalIntervalSeries const* intervals,
                                  std::string const& filepath,
                                  std::string const& delimiter) {
    return writeCSVTwoColumn(intervals, filepath, delimiter, true, 
                             "Start" + delimiter + "End");
}

/**
 * @brief Helper to write DigitalIntervalSeries without header
 * 
 * @param intervals The DigitalIntervalSeries to write
 * @param filepath Output file path
 * @param delimiter Column delimiter
 * @return true if write succeeded
 */
inline bool writeCSVNoHeader(DigitalIntervalSeries const* intervals,
                             std::string const& filepath,
                             std::string const& delimiter = ",") {
    return writeCSVTwoColumn(intervals, filepath, delimiter, false, "");
}

//=============================================================================
// Pre-configured test interval series for CSV loading tests
//=============================================================================

/**
 * @brief Simple interval series with 5 non-overlapping intervals
 * 
 * Creates intervals at: [10,25], [50,75], [100,150], [200,220], [300,350]
 */
inline std::shared_ptr<DigitalIntervalSeries> simple_intervals() {
    return DigitalIntervalSeriesBuilder()
        .withInterval(10, 25)
        .withInterval(50, 75)
        .withInterval(100, 150)
        .withInterval(200, 220)
        .withInterval(300, 350)
        .build();
}

/**
 * @brief Single interval for minimal test case
 * 
 * Creates one interval at: [0, 100]
 */
inline std::shared_ptr<DigitalIntervalSeries> single_interval() {
    return DigitalIntervalSeriesBuilder()
        .withInterval(0, 100)
        .build();
}

/**
 * @brief Many short intervals using regular pattern
 * 
 * Creates 10 intervals of duration 10 with gap 5 between them
 * Pattern: [0,10], [15,25], [30,40], ...
 */
inline std::shared_ptr<DigitalIntervalSeries> regular_pattern_intervals() {
    return DigitalIntervalSeriesBuilder()
        .withPattern(0, 150, 10, 5)  // 10 duration, 5 gap
        .build();
}

/**
 * @brief Intervals with large time values
 * 
 * Tests handling of large 64-bit time values
 */
inline std::shared_ptr<DigitalIntervalSeries> large_time_intervals() {
    return DigitalIntervalSeriesBuilder()
        .withInterval(1000000, 1000100)
        .withInterval(2000000, 2000500)
        .withInterval(5000000, 5001000)
        .build();
}

/**
 * @brief Adjacent (touching) intervals
 * 
 * Creates intervals that share endpoints: [0,10], [10,20], [20,30]
 * Note: These are NOT overlapping since end is exclusive in many interpretations
 */
inline std::shared_ptr<DigitalIntervalSeries> adjacent_intervals() {
    return DigitalIntervalSeriesBuilder()
        .withInterval(0, 10)
        .withInterval(10, 20)
        .withInterval(20, 30)
        .withInterval(30, 40)
        .build();
}

/**
 * @brief Short duration intervals (1 unit each)
 * 
 * Tests minimal duration intervals
 */
inline std::shared_ptr<DigitalIntervalSeries> minimal_duration_intervals() {
    return DigitalIntervalSeriesBuilder()
        .withInterval(5, 6)
        .withInterval(10, 11)
        .withInterval(20, 21)
        .withInterval(50, 51)
        .build();
}

/**
 * @brief Wide range of interval durations
 * 
 * Tests handling of varying interval sizes
 */
inline std::shared_ptr<DigitalIntervalSeries> varied_duration_intervals() {
    return DigitalIntervalSeriesBuilder()
        .withInterval(0, 1)       // Duration: 1
        .withInterval(10, 20)     // Duration: 10
        .withInterval(50, 150)    // Duration: 100
        .withInterval(200, 1200)  // Duration: 1000
        .build();
}

} // namespace digital_interval_scenarios

#endif // DIGITAL_INTERVAL_CSV_LOADING_SCENARIOS_HPP
