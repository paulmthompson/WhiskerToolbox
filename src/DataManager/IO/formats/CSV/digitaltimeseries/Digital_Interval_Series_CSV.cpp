#include "Digital_Interval_Series_CSV.hpp"

#include "DigitalTimeSeries/Digital_Interval_Series.hpp" // Required for DigitalIntervalSeries
//#include "loaders/loading_utils.hpp"


#include <fstream>
#include <sstream>
#include <filesystem> // Required for std::filesystem
#include <iostream>   // Required for std::cout, std::cerr

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
        size_t delimiter_pos = csv_line.find(delimiter);
        if (delimiter_pos == std::string::npos) {
            std::cerr << "Warning: No delimiter found in line: " << csv_line << std::endl;
            continue;
        }
        
        try {
            std::string start_str = csv_line.substr(0, delimiter_pos);
            std::string end_str = csv_line.substr(delimiter_pos + 1);
            
            int64_t start = std::stoll(start_str);
            int64_t end = std::stoll(end_str);
            
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
        int max_column = std::max(options.start_column, options.end_column);
        if (static_cast<int>(tokens.size()) <= max_column) {
            std::cerr << "Warning: Line has insufficient columns (expected at least " 
                      << (max_column + 1) << ", got " << tokens.size() << "): " << line << std::endl;
            continue;
        }

        try {
            int64_t start = std::stoll(tokens[static_cast<size_t>(options.start_column)]);
            int64_t end = std::stoll(tokens[static_cast<size_t>(options.end_column)]);
            
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

void save(
        DigitalIntervalSeries const * interval_data,
        CSVIntervalSaverOptions const & opts) {
    
    if (!interval_data) {
        std::cerr << "Error: DigitalIntervalSeries data is null. Cannot save." << std::endl;
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

    auto const intervals = interval_data->view();

    for (auto const & interval : intervals) {
        // Using interval.start and interval.end directly as they are int64_t
        fout << interval.value().start << opts.delimiter << interval.value().end << opts.line_delim;
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
        std::cout << "Successfully saved digital interval series to " << full_path << std::endl;
    }
}
