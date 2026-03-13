#include "Digital_Interval_Series_CSV.hpp"

#include "DigitalTimeSeries/Digital_Interval_Series.hpp"// Required for DigitalIntervalSeries
#include "IO/core/AtomicWrite.hpp"

#include <cassert>
#include <filesystem>
#include <fstream>
#include <iostream>// Required for std::cout, std::cerr
#include <sstream>

bool save(
        DigitalIntervalSeries const * interval_data,
        CSVIntervalSaverOptions const & opts) {
    assert(interval_data && "save: interval_data must not be null");

    auto const target_path = std::filesystem::path(opts.parent_dir) / opts.filename;

    bool const ok = atomicWriteFile(target_path, [&](std::ostream & out) {
        if (opts.save_header && !opts.header.empty()) {
            out << opts.header << opts.line_delim;
        }

        for (auto const & interval: interval_data->view()) {
            out << interval.value().start << opts.delimiter << interval.value().end << opts.line_delim;
        }
        return out.good();
    });

    if (ok) {
        std::cout << "Successfully saved digital interval series to " << target_path
                  << " (" << interval_data->size() << " intervals)" << std::endl;
    }
    return ok;
}


std::vector<Interval> load_digital_series_from_csv(
        std::string const & filename,
        char delimiter) {
    std::string csv_line;

    std::fstream myfile;
    myfile.open(filename, std::fstream::in);

    if (!myfile.is_open()) {
        std::cerr << "Error loading digital series: File " << filename << " not found." << std::endl;
        return {};
    }

    auto output = std::vector<Interval>();
    while (getline(myfile, csv_line)) {
        if (csv_line.empty()) continue;

        // Split by delimiter
        size_t const delimiter_pos = csv_line.find(delimiter);
        if (delimiter_pos == std::string::npos) {
            std::cerr << "Warning: No delimiter found in line: " << csv_line << std::endl;
            continue;
        }

        try {
            std::string const start_str = csv_line.substr(0, delimiter_pos);
            std::string const end_str = csv_line.substr(delimiter_pos + 1);

            int64_t const start = std::stoll(start_str);
            int64_t const end = std::stoll(end_str);

            output.emplace_back(Interval{start, end});
        } catch (std::exception const & e) {
            std::cerr << "Warning: Could not parse line: " << csv_line << " - " << e.what() << std::endl;
            continue;
        }
    }

    return output;
}

std::vector<Interval> load(CSVIntervalLoaderOptions const & options) {
    std::fstream file;
    file.open(options.filepath, std::fstream::in);

    if (!file.is_open()) {
        std::cerr << "Error loading digital interval series: File " << options.filepath << " not found." << std::endl;
        return {};
    }

    std::vector<Interval> output;
    std::string line;
    bool first_line = true;

    while (std::getline(file, line)) {
        // Skip header if present
        if (first_line && options.has_header) {
            first_line = false;
            continue;
        }
        first_line = false;

        // Skip empty lines
        if (line.empty()) {
            continue;
        }

        // Parse the line
        std::vector<std::string> tokens;
        std::stringstream ss(line);
        std::string token;

        // Split by delimiter
        while (std::getline(ss, token, options.delimiter[0])) {
            tokens.push_back(token);
        }

        // Validate we have enough columns
        int const max_column = std::max(options.start_column, options.end_column);
        if (static_cast<int>(tokens.size()) <= max_column) {
            std::cerr << "Warning: Line has insufficient columns (expected at least "
                      << (max_column + 1) << ", got " << tokens.size() << "): " << line << std::endl;
            continue;
        }

        try {
            int64_t const start = std::stoll(tokens[static_cast<size_t>(options.start_column)]);
            int64_t const end = std::stoll(tokens[static_cast<size_t>(options.end_column)]);

            if (start > end) {
                std::cerr << "Warning: Start time (" << start << ") is greater than end time ("
                          << end << ") on line: " << line << std::endl;
                continue;
            }

            output.emplace_back(Interval{start, end});
        } catch (std::exception const & e) {
            std::cerr << "Warning: Failed to parse line: " << line << " - " << e.what() << std::endl;
            continue;
        }
    }

    file.close();
    std::cout << "Successfully loaded " << output.size() << " intervals from " << options.filepath << std::endl;
    return output;
}
