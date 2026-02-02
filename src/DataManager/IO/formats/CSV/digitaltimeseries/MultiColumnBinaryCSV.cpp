#include "MultiColumnBinaryCSV.hpp"

#include "DigitalTimeSeries/Digital_Interval_Series.hpp"
#include "TimeFrame/TimeFrame.hpp"

#include <fstream>
#include <iostream>
#include <sstream>
#include <vector>
#include <algorithm>

namespace {

/**
 * @brief Split a line by delimiter, handling both single-char and multi-char delimiters
 */
std::vector<std::string> splitLine(std::string const & line, std::string const & delimiter) {
    std::vector<std::string> tokens;
    
    if (delimiter.length() == 1) {
        // Fast path for single-character delimiter
        std::stringstream ss(line);
        std::string token;
        while (std::getline(ss, token, delimiter[0])) {
            // Trim whitespace from token
            size_t start = token.find_first_not_of(" \t\r\n");
            size_t end = token.find_last_not_of(" \t\r\n");
            if (start != std::string::npos && end != std::string::npos) {
                tokens.push_back(token.substr(start, end - start + 1));
            } else if (!token.empty()) {
                tokens.push_back("");
            }
        }
    } else {
        // Multi-character delimiter
        size_t pos = 0;
        size_t prev = 0;
        while ((pos = line.find(delimiter, prev)) != std::string::npos) {
            std::string token = line.substr(prev, pos - prev);
            // Trim whitespace
            size_t start = token.find_first_not_of(" \t\r\n");
            size_t end = token.find_last_not_of(" \t\r\n");
            if (start != std::string::npos && end != std::string::npos) {
                tokens.push_back(token.substr(start, end - start + 1));
            } else {
                tokens.push_back("");
            }
            prev = pos + delimiter.length();
        }
        // Last token
        std::string token = line.substr(prev);
        size_t start = token.find_first_not_of(" \t\r\n");
        size_t end = token.find_last_not_of(" \t\r\n");
        if (start != std::string::npos && end != std::string::npos) {
            tokens.push_back(token.substr(start, end - start + 1));
        }
    }
    
    return tokens;
}

/**
 * @brief Parse binary values from a data column and extract intervals
 * 
 * @param values Binary values (0 or 1)
 * @return Vector of intervals where value was 1
 */
std::vector<Interval> extractIntervalsFromBinaryData(std::vector<int> const & values) {
    std::vector<Interval> intervals;
    
    if (values.empty()) {
        return intervals;
    }
    
    bool in_interval = false;
    int64_t start_idx = 0;
    
    for (size_t i = 0; i < values.size(); ++i) {
        if (values[i] == 1 && !in_interval) {
            // Start of interval
            in_interval = true;
            start_idx = static_cast<int64_t>(i);
        } else if (values[i] == 0 && in_interval) {
            // End of interval
            in_interval = false;
            intervals.push_back(Interval{start_idx, static_cast<int64_t>(i - 1)});
        }
    }
    
    // Handle case where last value is 1 (interval extends to end)
    if (in_interval) {
        intervals.push_back(Interval{start_idx, static_cast<int64_t>(values.size() - 1)});
    }
    
    return intervals;
}

} // anonymous namespace


std::vector<std::string> getColumnNames(
    std::string const & filepath,
    int header_lines_to_skip,
    std::string const & delimiter
) {
    std::ifstream file(filepath);
    if (!file.is_open()) {
        std::cerr << "Error: Cannot open file: " << filepath << std::endl;
        return {};
    }
    
    std::string line;
    
    // Skip header lines
    for (int i = 0; i < header_lines_to_skip; ++i) {
        if (!std::getline(file, line)) {
            std::cerr << "Error: File has fewer than " << header_lines_to_skip 
                      << " header lines" << std::endl;
            return {};
        }
    }
    
    // Read the column header line
    if (!std::getline(file, line)) {
        std::cerr << "Error: Cannot read column header line" << std::endl;
        return {};
    }
    
    return splitLine(line, delimiter);
}


std::shared_ptr<DigitalIntervalSeries> load(MultiColumnBinaryCSVLoaderOptions const & opts) {
    std::ifstream file(opts.filepath);
    if (!file.is_open()) {
        std::cerr << "Error: Cannot open file: " << opts.filepath << std::endl;
        return nullptr;
    }
    
    int const header_lines = opts.getHeaderLinesToSkip();
    int const data_col = opts.getDataColumn();
    std::string const delimiter = opts.getDelimiter();
    double const threshold = opts.getBinaryThreshold();
    
    std::string line;
    
    // Skip header lines
    for (int i = 0; i < header_lines; ++i) {
        if (!std::getline(file, line)) {
            std::cerr << "Error: File has fewer than " << header_lines 
                      << " header lines" << std::endl;
            return nullptr;
        }
    }
    
    // Skip the column header line
    if (!std::getline(file, line)) {
        std::cerr << "Error: Cannot read column header line" << std::endl;
        return nullptr;
    }
    
    // Read data rows
    std::vector<int> binary_values;
    int line_num = header_lines + 1;
    
    while (std::getline(file, line)) {
        line_num++;
        
        // Skip empty lines
        if (line.empty() || line.find_first_not_of(" \t\r\n") == std::string::npos) {
            continue;
        }
        
        auto tokens = splitLine(line, delimiter);
        
        // Validate column exists
        if (static_cast<int>(tokens.size()) <= data_col) {
            std::cerr << "Warning: Line " << line_num << " has insufficient columns (expected "
                      << (data_col + 1) << ", got " << tokens.size() << ")" << std::endl;
            continue;
        }
        
        try {
            double value = std::stod(tokens[static_cast<size_t>(data_col)]);
            binary_values.push_back(value >= threshold ? 1 : 0);
        } catch (std::exception const & e) {
            std::cerr << "Warning: Failed to parse value on line " << line_num 
                      << ": " << e.what() << std::endl;
            continue;
        }
    }
    
    std::cout << "Parsed " << binary_values.size() << " data values from column " << data_col << std::endl;
    
    // Extract intervals from binary data
    auto intervals = extractIntervalsFromBinaryData(binary_values);
    
    std::cout << "Extracted " << intervals.size() << " intervals from binary data" << std::endl;
    
    return std::make_shared<DigitalIntervalSeries>(intervals);
}


std::shared_ptr<TimeFrame> load(MultiColumnBinaryCSVTimeFrameOptions const & opts) {
    std::ifstream file(opts.filepath);
    if (!file.is_open()) {
        std::cerr << "Error: Cannot open file: " << opts.filepath << std::endl;
        return nullptr;
    }
    
    int const header_lines = opts.getHeaderLinesToSkip();
    int const time_col = opts.getTimeColumn();
    std::string const delimiter = opts.getDelimiter();
    double const sampling_rate = opts.getSamplingRate();
    
    std::string line;
    
    // Skip header lines
    for (int i = 0; i < header_lines; ++i) {
        if (!std::getline(file, line)) {
            std::cerr << "Error: File has fewer than " << header_lines 
                      << " header lines" << std::endl;
            return nullptr;
        }
    }
    
    // Skip the column header line
    if (!std::getline(file, line)) {
        std::cerr << "Error: Cannot read column header line" << std::endl;
        return nullptr;
    }
    
    // Read data rows
    std::vector<int> time_values;
    int line_num = header_lines + 1;
    
    while (std::getline(file, line)) {
        line_num++;
        
        // Skip empty lines
        if (line.empty() || line.find_first_not_of(" \t\r\n") == std::string::npos) {
            continue;
        }
        
        auto tokens = splitLine(line, delimiter);
        
        // Validate column exists
        if (static_cast<int>(tokens.size()) <= time_col) {
            std::cerr << "Warning: Line " << line_num << " has insufficient columns (expected "
                      << (time_col + 1) << ", got " << tokens.size() << ")" << std::endl;
            continue;
        }
        
        try {
            double time_value = std::stod(tokens[static_cast<size_t>(time_col)]);
            // Convert to integer using sampling rate
            int int_time = static_cast<int>(time_value * sampling_rate);
            time_values.push_back(int_time);
        } catch (std::exception const & e) {
            std::cerr << "Warning: Failed to parse time value on line " << line_num 
                      << ": " << e.what() << std::endl;
            continue;
        }
    }
    
    std::cout << "Loaded " << time_values.size() << " time values (sampling rate: " 
              << sampling_rate << " Hz)" << std::endl;
    
    if (time_values.empty()) {
        std::cerr << "Error: No valid time values found" << std::endl;
        return nullptr;
    }
    
    return std::make_shared<TimeFrame>(time_values);
}
