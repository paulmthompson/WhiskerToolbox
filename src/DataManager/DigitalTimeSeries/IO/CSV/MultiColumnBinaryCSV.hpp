#ifndef MULTI_COLUMN_BINARY_CSV_HPP
#define MULTI_COLUMN_BINARY_CSV_HPP

#include "datamanager_export.h"
#include "utils/LoaderOptionsConcepts.hpp"
#include "TimeFrame/interval_data.hpp"

#include <rfl.hpp>
#include <rfl/json.hpp>

#include <memory>
#include <optional>
#include <string>
#include <vector>

class DigitalIntervalSeries;
class TimeFrame;

/**
 * @brief Options for loading multi-column binary event data from CSV-like files
 * 
 * This loader handles files with the following format:
 * - Multiple header lines (date, time, blank lines) that can be skipped
 * - A column header row with column names
 * - Data rows with a time column and multiple binary event columns (0/1 values)
 * 
 * Example file format:
 * @code
 * 11/28/2025
 * 10:23:25 AM
 * 
 * 
 * Time    v0    v1    v2    v3    y0    y1    y2    y3
 * 0.000000    1.000000    0.000000    0.000000    0.000000
 * 0.000071    1.000000    0.000000    0.000000    0.000000
 * @endcode
 * 
 * Uses reflect-cpp for automatic JSON serialization/deserialization.
 */
struct DATAMANAGER_EXPORT MultiColumnBinaryCSVLoaderOptions {
    /// Path to the CSV file (required)
    std::string filepath;
    
    /// Number of header lines to skip before reaching column headers (default: 5)
    /// This includes date, time, and any blank lines before the header row
    std::optional<rfl::Validator<int, rfl::Minimum<0>>> header_lines_to_skip;
    
    /// Column index (0-based) containing time values (default: 0)
    std::optional<rfl::Validator<int, rfl::Minimum<0>>> time_column;
    
    /// Column index (0-based) containing binary event data to extract (default: 1)
    /// This is the column that will be converted to intervals
    std::optional<rfl::Validator<int, rfl::Minimum<0>>> data_column;
    
    /// Delimiter between columns (default: "\t" for tab-separated)
    std::optional<std::string> delimiter;
    
    /// Sampling rate in Hz for converting fractional time to integer indices
    /// If provided, time values are multiplied by this rate to get integer indices
    /// If not provided (or 0), row indices are used directly as TimeFrameIndex values
    std::optional<rfl::Validator<double, rfl::Minimum<0.0>>> sampling_rate;
    
    /// Threshold for considering a value as "on" (default: 0.5)
    /// Values >= threshold are considered 1, values < threshold are considered 0
    std::optional<double> binary_threshold;
    
    // Helper methods to get values with defaults
    int getHeaderLinesToSkip() const { 
        return header_lines_to_skip.has_value() ? header_lines_to_skip.value().value() : 5; 
    }
    int getTimeColumn() const { 
        return time_column.has_value() ? time_column.value().value() : 0; 
    }
    int getDataColumn() const { 
        return data_column.has_value() ? data_column.value().value() : 1; 
    }
    std::string getDelimiter() const { 
        return delimiter.value_or("\t"); 
    }
    double getSamplingRate() const { 
        return sampling_rate.has_value() ? sampling_rate.value().value() : 0.0; 
    }
    double getBinaryThreshold() const { 
        return binary_threshold.value_or(0.5); 
    }
};

// Compile-time validation
static_assert(WhiskerToolbox::ValidLoaderOptions<MultiColumnBinaryCSVLoaderOptions>,
    "MultiColumnBinaryCSVLoaderOptions must have 'filepath' field and must not have 'data_type' or 'name' fields");


/**
 * @brief Options for loading TimeFrame from multi-column binary CSV files
 * 
 * Extracts the time column and converts to integer time values using
 * the specified sampling rate.
 */
struct DATAMANAGER_EXPORT MultiColumnBinaryCSVTimeFrameOptions {
    /// Path to the CSV file (required)
    std::string filepath;
    
    /// Number of header lines to skip before reaching column headers (default: 5)
    std::optional<rfl::Validator<int, rfl::Minimum<0>>> header_lines_to_skip;
    
    /// Column index (0-based) containing time values (default: 0)
    std::optional<rfl::Validator<int, rfl::Minimum<0>>> time_column;
    
    /// Delimiter between columns (default: "\t" for tab-separated)
    std::optional<std::string> delimiter;
    
    /// Sampling rate in Hz for converting fractional time to integer indices
    /// Time values are multiplied by this rate to get integer time values
    std::optional<rfl::Validator<double, rfl::Minimum<0.0>>> sampling_rate;
    
    // Helper methods to get values with defaults
    int getHeaderLinesToSkip() const { 
        return header_lines_to_skip.has_value() ? header_lines_to_skip.value().value() : 5; 
    }
    int getTimeColumn() const { 
        return time_column.has_value() ? time_column.value().value() : 0; 
    }
    std::string getDelimiter() const { 
        return delimiter.value_or("\t"); 
    }
    double getSamplingRate() const { 
        return sampling_rate.has_value() ? sampling_rate.value().value() : 1.0; 
    }
};

// Compile-time validation
static_assert(WhiskerToolbox::ValidLoaderOptions<MultiColumnBinaryCSVTimeFrameOptions>,
    "MultiColumnBinaryCSVTimeFrameOptions must have 'filepath' field and must not have 'data_type' or 'name' fields");


/**
 * @brief Load digital interval series from a multi-column binary CSV file
 * 
 * Parses the specified data column, finds contiguous regions where value >= threshold,
 * and converts them to intervals. The time indices are based on row position
 * or calculated from the time column using the sampling rate.
 * 
 * @param opts Configuration options for loading
 * @return Shared pointer to DigitalIntervalSeries, or nullptr on error
 */
std::shared_ptr<DigitalIntervalSeries> DATAMANAGER_EXPORT load(MultiColumnBinaryCSVLoaderOptions const & opts);

/**
 * @brief Load TimeFrame from a multi-column binary CSV file
 * 
 * Extracts the time column and converts fractional time values to integers
 * using the specified sampling rate.
 * 
 * @param opts Configuration options for loading
 * @return Shared pointer to TimeFrame, or nullptr on error
 */
std::shared_ptr<TimeFrame> DATAMANAGER_EXPORT load(MultiColumnBinaryCSVTimeFrameOptions const & opts);

/**
 * @brief Extract column names from a multi-column binary CSV file
 * 
 * Useful for inspecting which columns are available in a file.
 * 
 * @param filepath Path to the CSV file
 * @param header_lines_to_skip Number of lines to skip before header row
 * @param delimiter Column delimiter
 * @return Vector of column names, empty on error
 */
std::vector<std::string> getColumnNames(
    std::string const & filepath,
    int header_lines_to_skip = 5,
    std::string const & delimiter = "\t"
);

#endif // MULTI_COLUMN_BINARY_CSV_HPP
