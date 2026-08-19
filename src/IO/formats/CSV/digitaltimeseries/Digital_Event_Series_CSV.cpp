#include "Digital_Event_Series_CSV.hpp"

#include "DigitalTimeSeries/Digital_Event_Series.hpp"
#include "IO/core/AtomicWrite.hpp"

#include <algorithm>
#include <cassert>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <map>
#include <numeric>
#include <regex>
#include <sstream>

namespace {

/**
 * @brief Attach an identity TimeFrame so stored indices map to matching ClockTicks.
 *
 * CSV persistence stores TimeFrameIndex values; callers may use view() after load.
 */
void assignIdentityTimeFrameForStoredEvents(DigitalEventSeries & series) {
    if (series.size() == 0) {
        return;
    }

    int64_t max_index = 0;
    for (std::size_t i = 0; i < series.size(); ++i) {
        max_index = std::max(max_index, series.getStoredEvent(i).getValue());
    }

    std::vector<int> times(static_cast<std::size_t>(max_index + 1));
    std::iota(times.begin(), times.end(), 0);
    series.setTimeFrame(std::make_shared<TimeFrame>(times));
}

}// namespace

std::vector<std::shared_ptr<DigitalEventSeries>> load(CSVEventLoaderOptions const & options) {
    std::vector<std::shared_ptr<DigitalEventSeries>> result;

    if (options.event_column < 0) {
        std::cerr << "Error loading digital event series: event_column must be non-negative, got "
                  << options.event_column << std::endl;
        return result;
    }

    std::ifstream file(options.filepath);
    if (!file.is_open()) {
        std::cerr << "Error loading digital event series: File " << options.filepath << " not found." << std::endl;
        return result;
    }

    std::string line;
    bool first_line = true;

    // Map to store events by identifier (for multi-column case)
    std::map<std::string, std::vector<TimeFrameIndex>> events_by_identifier;

    // Vector to store events (for single column case)
    std::vector<TimeFrameIndex> single_events;

    bool const has_identifier_column = (options.identifier_column >= 0);

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
        int const required_columns = std::max(options.event_column,
                                              has_identifier_column ? options.identifier_column : -1) +
                                     1;
        if (static_cast<int>(tokens.size()) < required_columns) {
            std::cerr << "Warning: Line has insufficient columns (expected at least "
                      << required_columns << ", got " << tokens.size() << "): " << line << std::endl;
            continue;
        }

        try {
            // Parse event timestamp
            float event_time_float = std::stof(tokens[static_cast<size_t>(options.event_column)]);

            // Apply scaling BEFORE conversion to integer
            // This is critical for timestamps like 0.01493 seconds that need to be
            // converted to sample indices (e.g., 0.01493 * 30000 = 447.9 → 448)
            if (options.scale != 1.0f) {
                if (options.scale_divide) {
                    event_time_float /= options.scale;
                } else {
                    event_time_float *= options.scale;
                }
            }

            TimeFrameIndex const event_time(static_cast<int64_t>(event_time_float));

            if (has_identifier_column) {
                // Multi-column case: group by identifier
                std::string const identifier = tokens[static_cast<size_t>(options.identifier_column)];
                events_by_identifier[identifier].push_back(event_time);
            } else {
                // Single column case: add to main vector
                single_events.push_back(event_time);
            }

        } catch (std::exception const & e) {
            std::cerr << "Warning: Failed to parse line: " << line << " - " << e.what() << std::endl;
            continue;
        }
    }

    file.close();

    // Create DigitalEventSeries objects
    if (has_identifier_column) {
        // Multi-column case: create one series per identifier
        for (auto const & [identifier, events]: events_by_identifier) {
            if (!events.empty()) {
                auto series = std::make_shared<DigitalEventSeries>(events);
                assignIdentityTimeFrameForStoredEvents(*series);
                result.push_back(series);
                std::cout << "Created event series '" << options.base_name << "_" << identifier
                          << "' with " << events.size() << " events" << std::endl;
            }
        }

        std::cout << "Successfully loaded " << result.size() << " event series from "
                  << options.filepath << std::endl;
    } else {
        // Single column case: create one series
        if (!single_events.empty()) {
            auto series = std::make_shared<DigitalEventSeries>(single_events);
            assignIdentityTimeFrameForStoredEvents(*series);
            result.push_back(series);
            std::cout << "Created event series '" << options.base_name
                      << "' with " << single_events.size() << " events" << std::endl;
        }

        std::cout << "Successfully loaded " << single_events.size() << " events from "
                  << options.filepath << std::endl;
    }

    return result;
}

std::vector<std::shared_ptr<DigitalEventSeries>> load(CSVEventMultiFileLoaderOptions const & options) {
    std::vector<std::shared_ptr<DigitalEventSeries>> result;

    if (options.event_column < 0) {
        std::cerr << "Error loading digital event series: event_column must be non-negative, got "
                  << options.event_column << std::endl;
        return result;
    }

    if (!std::filesystem::exists(options.parent_dir)) {
        std::cerr << "Error loading digital event series: Directory does not exist: "
                  << options.parent_dir << std::endl;
        return result;
    }

    std::vector<std::filesystem::path> matched_files;
    auto const matches_pattern = [&](std::string const & filename) {
        if (options.file_pattern.empty()) {
            return filename.size() >= 4 &&
                   (filename.ends_with(".csv") || filename.ends_with(".txt"));
        }

        std::string const regex_pattern =
                std::regex_replace(options.file_pattern, std::regex("\\*"), ".*");
        std::regex const file_regex(regex_pattern);
        return std::regex_match(filename, file_regex);
    };

    for (auto const & entry: std::filesystem::directory_iterator(options.parent_dir)) {
        if (!entry.is_regular_file()) {
            continue;
        }

        std::string const filename = entry.path().filename().string();
        if (matches_pattern(filename)) {
            matched_files.push_back(entry.path());
        }
    }

    if (matched_files.empty()) {
        std::cerr << "Error loading digital event series: No matching files in directory: "
                  << options.parent_dir << std::endl;
        return result;
    }

    std::sort(matched_files.begin(), matched_files.end());

    for (auto const & file_path: matched_files) {
        CSVEventLoaderOptions single_opts;
        single_opts.filepath = file_path.string();
        single_opts.delimiter = options.delimiter;
        single_opts.has_header = options.has_header;
        single_opts.event_column = options.event_column;
        single_opts.identifier_column = -1;
        single_opts.base_name = options.base_name;
        single_opts.scale = options.scale;
        single_opts.scale_divide = options.scale_divide;

        auto const loaded = load(single_opts);
        if (loaded.empty()) {
            std::cerr << "Warning: No events loaded from file: " << file_path.string() << std::endl;
            continue;
        }

        if (loaded.size() > 1) {
            std::cerr << "Warning: File " << file_path.string() << " produced "
                      << loaded.size() << " series; using the first" << std::endl;
        }

        result.push_back(loaded.front());
    }

    if (result.empty()) {
        std::cerr << "Error loading digital event series: No events loaded from directory: "
                  << options.parent_dir << std::endl;
    } else {
        std::cout << "Successfully loaded " << result.size() << " event series from directory "
                  << options.parent_dir << std::endl;
    }

    return result;
}

bool save(DigitalEventSeries const * event_data, CSVEventSaverOptions const & opts) {
    assert(event_data && "save: event_data must not be null");

    auto const target_path = std::filesystem::path(opts.parent_dir) / opts.filename;

    bool const ok = atomicWriteFile(target_path, [&](std::ostream & out) {
        if (opts.save_header && !opts.header.empty()) {
            out << opts.header << opts.line_delim;
        }

        out << std::fixed << std::setprecision(opts.precision);

        for (std::size_t i = 0; i < event_data->size(); ++i) {
            out << event_data->getStoredEvent(i).getValue() << opts.line_delim;
        }
        return out.good();
    });

    if (ok) {
        std::cout << "Successfully saved digital event series to " << target_path
                  << " (" << event_data->size() << " events)" << std::endl;
    }
    return ok;
}