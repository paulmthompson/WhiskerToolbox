#include "Digital_Event_Series_CSV.hpp"

#include "DigitalTimeSeries/Digital_Event_Series.hpp"

#include <fstream>
#include <sstream>
#include <filesystem>
#include <iostream>
#include <map>
#include <iomanip>

template <typename T>
std::optional<std::string> check_dir_and_get_full_path(T opts) {
    if (!opts.parent_dir.empty() && !std::filesystem::exists(opts.parent_dir)) {
        try {
            std::filesystem::create_directories(opts.parent_dir);
            std::cout << "Created directory: " << opts.parent_dir << std::endl;
        } catch (std::filesystem::filesystem_error const& e) {
            std::cerr << "Error creating directory " << opts.parent_dir << ": " << e.what() << std::endl;
            return std::nullopt;
        }
    }

    std::string full_path = opts.parent_dir.empty() ? opts.filename : (std::filesystem::path(opts.parent_dir) / opts.filename).string();
    return full_path;
}

std::vector<std::shared_ptr<DigitalEventSeries>> load(CSVEventLoaderOptions const & options) {
    std::vector<std::shared_ptr<DigitalEventSeries>> result;
    
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
    
    bool has_identifier_column = (options.identifier_column >= 0);

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
        int required_columns = std::max(options.event_column, 
                                       has_identifier_column ? options.identifier_column : -1) + 1;
        if (static_cast<int>(tokens.size()) < required_columns) {
            std::cerr << "Warning: Line has insufficient columns (expected at least " 
                      << required_columns << ", got " << tokens.size() << "): " << line << std::endl;
            continue;
        }

        try {
            // Parse event timestamp
            float event_time_float = std::stof(tokens[static_cast<size_t>(options.event_column)]);
            TimeFrameIndex event_time(static_cast<int64_t>(event_time_float));
            
            if (has_identifier_column) {
                // Multi-column case: group by identifier
                std::string identifier = tokens[static_cast<size_t>(options.identifier_column)];
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
        for (auto const & [identifier, events] : events_by_identifier) {
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

void save(DigitalEventSeries const * event_data, CSVEventSaverOptions const & opts) {
    if (!event_data) {
        std::cerr << "Error: DigitalEventSeries data is null. Cannot save." << std::endl;
        return;
    }

    auto result = check_dir_and_get_full_path(opts);
    if (!result.has_value()) {
        return;
    }

    std::string const full_path = result.value();

    std::ofstream fout;
    fout.open(full_path, std::ios_base::out | std::ios_base::trunc);

    if (!fout.is_open()) {
        std::cerr << "Error: Could not open file for writing: " << full_path << std::endl;
        return;
    }

    if (opts.save_header && !opts.header.empty()) {
        fout << opts.header << opts.line_delim;
    }

    auto const & events = event_data->view();

    // Set precision for floating point output
    fout << std::fixed << std::setprecision(opts.precision);

    for (auto const & event_time : events) {
        fout << event_time.time().getValue() << opts.line_delim;
        if (fout.fail()) {
            std::cerr << "Error: Failed while writing data to file: " << full_path << std::endl;
            fout.close();
            return;
        }
    }

    fout.close();
    if (fout.fail()) {
        std::cerr << "Error: Failed to properly close file: " << full_path << std::endl;
    } else {
        std::cout << "Successfully saved digital event series to " << full_path 
                  << " (" << events.size() << " events)" << std::endl;
    }
} 