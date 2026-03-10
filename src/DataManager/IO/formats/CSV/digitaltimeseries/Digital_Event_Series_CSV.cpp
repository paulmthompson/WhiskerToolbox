#include "Digital_Event_Series_CSV.hpp"

#include "DigitalTimeSeries/Digital_Event_Series.hpp"
#include "IO/core/AtomicWrite.hpp"

#include <cassert>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <map>
#include <sstream>

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
            result.push_back(series);
            std::cout << "Created event series '" << options.base_name
                      << "' with " << single_events.size() << " events" << std::endl;
        }

        std::cout << "Successfully loaded " << single_events.size() << " events from "
                  << options.filepath << std::endl;
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

        for (auto const & event: event_data->view()) {
            out << event.time().getValue() << opts.line_delim;
        }
        return out.good();
    });

    if (ok) {
        std::cout << "Successfully saved digital event series to " << target_path
                  << " (" << event_data->size() << " events)" << std::endl;
    }
    return ok;
}