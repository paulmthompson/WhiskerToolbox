#ifndef ANALOG_CSV_LOADING_SCENARIOS_HPP
#define ANALOG_CSV_LOADING_SCENARIOS_HPP

#include "fixtures/builders/AnalogTimeSeriesBuilder.hpp"

#include <filesystem>
#include <fstream>
#include <iomanip>
#include <memory>
#include <sstream>
#include <string>
#include <vector>

namespace analog_scenarios {

/**
 * @brief Helper to write AnalogTimeSeries data to CSV with time and value columns
 * 
 * Writes two-column CSV with optional header. Time column contains
 * TimeFrameIndex values, Data column contains float values.
 * 
 * @param signal The AnalogTimeSeries to write
 * @param filepath Output file path
 * @param delimiter Column delimiter (default ",")
 * @param write_header Whether to write header row
 * @param header_text Header text if write_header is true
 * @param precision Decimal precision for float values
 * @return true if write succeeded
 */
inline bool writeCSVTwoColumn(AnalogTimeSeries const* signal,
                              std::string const& filepath,
                              std::string const& delimiter = ",",
                              bool write_header = true,
                              std::string const& header_text = "Time,Data",
                              int precision = 6) {
    std::ofstream file(filepath);
    if (!file.is_open()) {
        return false;
    }
    
    file << std::fixed << std::setprecision(precision);
    
    // Write header if requested
    if (write_header) {
        file << header_text << "\n";
    }
    
    // Write data rows
    for (auto const& sample : signal->getAllSamples()) {
        file << sample.time_frame_index.getValue() << delimiter << sample.value() << "\n";
    }
    
    return file.good();
}

/**
 * @brief Helper to write AnalogTimeSeries data to single-column CSV (no time)
 * 
 * Writes a single column of values. Useful for testing single-column loading
 * where time indices are inferred from row number.
 * 
 * @param signal The AnalogTimeSeries to write
 * @param filepath Output file path
 * @param write_header Whether to write header row
 * @param header_text Header text if write_header is true
 * @param precision Decimal precision for float values
 * @return true if write succeeded
 */
inline bool writeCSVSingleColumn(AnalogTimeSeries const* signal,
                                 std::string const& filepath,
                                 bool write_header = false,
                                 std::string const& header_text = "Data",
                                 int precision = 6) {
    std::ofstream file(filepath);
    if (!file.is_open()) {
        return false;
    }
    
    file << std::fixed << std::setprecision(precision);
    
    // Write header if requested
    if (write_header) {
        file << header_text << "\n";
    }
    
    // Write data rows (values only)
    for (auto const& sample : signal->getAllSamples()) {
        file << sample.value() << "\n";
    }
    
    return file.good();
}

/**
 * @brief Helper to write AnalogTimeSeries with custom column order
 * 
 * Writes CSV with data column first, then time column (reversed order).
 * Useful for testing column index configuration.
 * 
 * @param signal The AnalogTimeSeries to write
 * @param filepath Output file path
 * @param delimiter Column delimiter
 * @param write_header Whether to write header row
 * @param precision Decimal precision for float values
 * @return true if write succeeded
 */
inline bool writeCSVReversedColumns(AnalogTimeSeries const* signal,
                                    std::string const& filepath,
                                    std::string const& delimiter = ",",
                                    bool write_header = true,
                                    int precision = 6) {
    std::ofstream file(filepath);
    if (!file.is_open()) {
        return false;
    }
    
    file << std::fixed << std::setprecision(precision);
    
    // Write header if requested (Data,Time order)
    if (write_header) {
        file << "Data" << delimiter << "Time" << "\n";
    }
    
    // Write data rows (value first, then time)
    for (auto const& sample : signal->getAllSamples()) {
        file << sample.value() << delimiter << sample.time_frame_index.getValue() << "\n";
    }
    
    return file.good();
}

/**
 * @brief Helper to write AnalogTimeSeries with custom delimiter
 * 
 * @param signal The AnalogTimeSeries to write
 * @param filepath Output file path
 * @param delimiter Column delimiter (tab, semicolon, etc.)
 * @param precision Decimal precision for float values
 * @return true if write succeeded
 */
inline bool writeCSVWithDelimiter(AnalogTimeSeries const* signal,
                                  std::string const& filepath,
                                  std::string const& delimiter,
                                  int precision = 6) {
    return writeCSVTwoColumn(signal, filepath, delimiter, true, 
                             "Time" + delimiter + "Data", precision);
}

//=============================================================================
// Pre-configured test signals for CSV loading tests
//=============================================================================

/**
 * @brief Simple integer value signal for exact CSV round-trip testing
 * 
 * Values: 10, 20, 30, 40, 50 at times 0, 1, 2, 3, 4
 * Good for verifying exact value preservation without floating point issues.
 */
inline std::shared_ptr<AnalogTimeSeries> simple_integer_values() {
    return AnalogTimeSeriesBuilder()
        .withValues({10.0f, 20.0f, 30.0f, 40.0f, 50.0f})
        .atTimes({0, 1, 2, 3, 4})
        .build();
}

/**
 * @brief Signal with floating point precision test values
 * 
 * Values designed to test precision handling in CSV format.
 */
inline std::shared_ptr<AnalogTimeSeries> precision_test_values() {
    return AnalogTimeSeriesBuilder()
        .withValues({1.234567f, 2.345678f, 3.456789f, 4.567890f, 5.678901f})
        .atTimes({0, 10, 20, 30, 40})
        .build();
}

/**
 * @brief Signal with non-sequential time indices
 * 
 * Tests that time values are correctly preserved, not inferred.
 */
inline std::shared_ptr<AnalogTimeSeries> non_sequential_times() {
    return AnalogTimeSeriesBuilder()
        .withValues({100.0f, 200.0f, 300.0f, 400.0f})
        .atTimes({5, 15, 100, 200})  // Non-sequential jumps
        .build();
}

/**
 * @brief Signal with negative values
 * 
 * Tests handling of negative numbers in CSV format.
 */
inline std::shared_ptr<AnalogTimeSeries> negative_values() {
    return AnalogTimeSeriesBuilder()
        .withValues({-10.5f, -5.25f, 0.0f, 5.25f, 10.5f})
        .atTimes({0, 1, 2, 3, 4})
        .build();
}

/**
 * @brief Larger signal for performance and edge case testing
 * 
 * 500 sample ramp from 0 to 500.
 */
inline std::shared_ptr<AnalogTimeSeries> ramp_500_samples() {
    return AnalogTimeSeriesBuilder()
        .withRamp(0, 499, 0.0f, 499.0f)
        .build();
}

/**
 * @brief Empty signal edge case
 * 
 * Tests handling of empty data files.
 */
inline std::shared_ptr<AnalogTimeSeries> empty_signal() {
    return AnalogTimeSeriesBuilder()
        .withValues({})
        .atTimes({})
        .build();
}

/**
 * @brief Single sample signal edge case
 */
inline std::shared_ptr<AnalogTimeSeries> single_sample() {
    return AnalogTimeSeriesBuilder()
        .withValues({42.5f})
        .atTimes({0})
        .build();
}

} // namespace analog_scenarios

#endif // ANALOG_CSV_LOADING_SCENARIOS_HPP
