#include "Line_Data_CSV.hpp"

#include "Lines/Line_Data.hpp"
#include "utils/string_manip.hpp"

#include <chrono>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <stdexcept>

void save_line_as_csv(Line2D const & line, std::string const & filename, int const point_precision) {
    std::fstream myfile;
    myfile.open(filename, std::fstream::out);

    myfile << std::fixed << std::setprecision(point_precision);
    for (auto & point: line) {
        myfile << point.x << "," << point.y << "\n";
    }

    myfile.close();
}

void save(
        LineData const * line_data,
        CSVSingleFileLineSaverOptions & opts) {

    //Check if directory exists
    if (!std::filesystem::exists(opts.parent_dir)) {
        std::filesystem::create_directories(opts.parent_dir);
        std::cout << "Created directory: " << opts.parent_dir << std::endl;
    }

    std::string filename = opts.parent_dir + "/" + opts.filename;

    std::ofstream file(filename);
    if (!file.is_open()) {
        throw std::runtime_error("Could not open file");
    }

    // Write the header
    if (opts.save_header) {
        file << opts.header << "\n";
    }

    // Write the data
    for (auto const & [time, entries]: line_data->getAllEntries()) {
        for (auto const & entry: entries) {
            std::ostringstream x_values;
            std::ostringstream y_values;

            for (auto const & point: entry.data) {
                x_values << std::fixed << std::setprecision(opts.precision) << point.x << opts.delimiter;
                y_values << std::fixed << std::setprecision(opts.precision) << point.y << opts.delimiter;
            }

            // Remove the trailing delimiter
            std::string x_str = x_values.str();
            std::string y_str = y_values.str();
            if (!x_str.empty()) x_str.pop_back();
            if (!y_str.empty()) y_str.pop_back();

            file << time.getValue() << ",\"" << x_str << "\",\"" << y_str << "\"\n";
        }
    }

    file.close();
}

void save(
        LineData const * line_data,
        CSVMultiFileLineSaverOptions & opts) {

    // Check if directory exists
    if (!std::filesystem::exists(opts.parent_dir)) {
        std::filesystem::create_directories(opts.parent_dir);
        std::cout << "Created directory: " << opts.parent_dir << std::endl;
    }

    int files_saved = 0;
    int files_skipped = 0;

    // Iterate through all timestamps with data
    for (auto const & [time, entries]: line_data->getAllEntries()) {
        // Only save if there are lines at this timestamp
        if (entries.empty()) {
            files_skipped++;
            continue;
        }

        // Only save the first line (index 0) as documented
        Line2D const & first_line = entries[0].data;
        
        // Generate filename with zero-padded frame number
        std::string const padded_frame = pad_frame_id(static_cast<int>(time.getValue()), opts.frame_id_padding);
        std::string const filename = opts.parent_dir + "/" + padded_frame + ".csv";

        // Check if file exists and handle according to overwrite setting
        bool file_exists = std::filesystem::exists(filename);
        if (file_exists && !opts.overwrite_existing) {
            std::cout << "Skipping existing file: " << filename << std::endl;
            files_skipped++;
            continue;
        }

        // Log if we're overwriting an existing file
        if (file_exists && opts.overwrite_existing) {
            std::cout << "Overwriting existing file: " << filename << std::endl;
        }

        std::ofstream file(filename);
        if (!file.is_open()) {
            std::cerr << "Warning: Could not open file " << filename << " for writing" << std::endl;
            files_skipped++;
            continue;
        }

        // Write the header if requested
        if (opts.save_header) {
            file << opts.header << opts.line_delim;
        }

        // Write X and Y coordinates in separate columns
        file << std::fixed << std::setprecision(opts.precision);
        for (auto const & point: first_line) {
            file << point.x << opts.delimiter << point.y << opts.line_delim;
        }

        file.close();
        files_saved++;
    }

    std::cout << "Multi-file CSV save complete: " << files_saved << " files saved";
    if (files_skipped > 0) {
        std::cout << ", " << files_skipped << " timestamps skipped (no lines or file errors)";
    }
    std::cout << std::endl;
}

std::vector<float> parse_string_to_float_vector(std::string const & str, std::string const & delimiter) {
    std::vector<float> result;
    
    // Reserve space to avoid reallocations - estimate based on string length
    // Assume average of 6 chars per number (including delimiter)
    result.reserve(str.length() / 6 + 1);

    char const delim_char = delimiter.empty() ? ',' : delimiter[0];
    char const * start = str.c_str();
    char const * end = start + str.length();
    char * parse_end;
    
    // Use strtof directly instead of creating string_view or substring
    while (start < end) {
        float value = std::strtof(start, &parse_end);
        if (parse_end == start) {
            // No conversion happened, skip character
            ++start;
            continue;
        }
        result.push_back(value);
        start = parse_end;
        
        // Skip delimiter
        if (start < end && *start == delim_char) {
            ++start;
        }
    }
    return result;
}

std::map<TimeFrameIndex, std::vector<Line2D>> load(CSVSingleFileLineLoaderOptions const & opts) {
    auto t1 = std::chrono::high_resolution_clock::now();
    std::map<TimeFrameIndex, std::vector<Line2D>> data_map;
    std::ifstream file(opts.filepath);
    if (!file.is_open()) {
        throw std::runtime_error("Could not open file: " + opts.filepath);
    }

    // Use larger buffer for better I/O performance
    constexpr size_t buffer_size = 1024 * 1024; // 1MB buffer
    std::vector<char> buffer(buffer_size);
    file.rdbuf()->pubsetbuf(buffer.data(), static_cast<std::streamsize>(buffer_size));

    std::string line;
    line.reserve(4096); // Reserve space for typical line length
    int loaded_lines = 0;
    bool is_first_line = true;
    
    while (std::getline(file, line)) {
        // Skip header if present
        if (is_first_line && opts.has_header) {
            is_first_line = false;
            if (line.find(opts.header_identifier) != std::string::npos) {
                continue;
            }
        }
        is_first_line = false;

        // Parse line manually to avoid multiple string copies
        size_t pos = 0;
        size_t comma_pos = line.find(opts.delimiter[0], pos);
        if (comma_pos == std::string::npos) {
            continue;
        }
        
        // Parse frame number
        int const frame_num = std::stoi(line.substr(pos, comma_pos - pos));
        pos = comma_pos + 1;
        
        // Find first quote for X coordinates
        size_t quote1 = line.find('"', pos);
        if (quote1 == std::string::npos) {
            continue;
        }
        size_t quote2 = line.find('"', quote1 + 1);
        if (quote2 == std::string::npos) {
            continue;
        }
        
        // Extract X coordinates string (avoiding copy by using pointer arithmetic)
        size_t x_start = quote1 + 1;
        size_t x_len = quote2 - x_start;
        
        // Find quotes for Y coordinates
        size_t quote3 = line.find('"', quote2 + 1);
        if (quote3 == std::string::npos) {
            continue;
        }
        size_t quote4 = line.find('"', quote3 + 1);
        if (quote4 == std::string::npos) {
            continue;
        }
        
        size_t y_start = quote3 + 1;
        size_t y_len = quote4 - y_start;
        
        // Parse coordinates directly from the line string to avoid copies
        std::string x_str = line.substr(x_start, x_len);
        std::string y_str = line.substr(y_start, y_len);

        std::vector<float> const x_values = parse_string_to_float_vector(x_str, opts.coordinate_delimiter);
        std::vector<float> const y_values = parse_string_to_float_vector(y_str, opts.coordinate_delimiter);

        if (x_values.size() != y_values.size()) {
            std::cerr << "Mismatched x and y values at frame: " << frame_num << std::endl;
            continue;
        }

        // Use emplace or operator[] to avoid extra lookup
        data_map[TimeFrameIndex(frame_num)].emplace_back(create_line(x_values, y_values));
        loaded_lines += 1;
    }

    file.close();
    auto t2 = std::chrono::high_resolution_clock::now();

    auto duration = std::chrono::duration<double>(t2 - t1).count();
    std::cout << "Loaded " << loaded_lines << " lines from " << opts.filepath << " in " << duration << "s" << std::endl;
    return data_map;
}

std::map<TimeFrameIndex, std::vector<Line2D>> load_line_csv(std::string const & filepath) {
    // Wrapper function for backward compatibility
    // Uses the new options-based load function with default settings
    CSVSingleFileLineLoaderOptions opts;
    opts.filepath = filepath;
    // All other options use their default values which match the original hardcoded behavior
    return load(opts);
}

Line2D load_line_from_csv(std::string const & filename) {
    std::string csv_line;

    auto line_output = Line2D();

    std::fstream myfile;
    myfile.open(filename, std::fstream::in);

    std::string x_str;
    std::string y_str;

    while (getline(myfile, csv_line)) {

        std::stringstream ss(csv_line);

        getline(ss, x_str, ',');
        getline(ss, y_str);

        //std::cout << x_str << " , " << y_str << std::endl;

        line_output.push_back(Point2D<float>{std::stof(x_str), std::stof(y_str)});
    }

    return line_output;
}

std::map<TimeFrameIndex, std::vector<Line2D>> load(CSVMultiFileLineLoaderOptions const & opts) {
    std::map<TimeFrameIndex, std::vector<Line2D>> data_map;
    
    // Check if directory exists
    if (!std::filesystem::exists(opts.parent_dir)) {
        std::cerr << "Error: Directory does not exist: " << opts.parent_dir << std::endl;
        return data_map;
    }

    int files_loaded = 0;
    int files_skipped = 0;

    // Iterate through all files in the directory
    for (auto const & entry : std::filesystem::directory_iterator(opts.parent_dir)) {
        if (!entry.is_regular_file()) {
            continue;
        }

        std::string const filename = entry.path().filename().string();
        
        // Check if file matches the pattern (simple check for .csv extension)
        if (filename.length() < 4 || filename.substr(filename.length() - 4) != ".csv") {
            continue;
        }

        // Extract frame number from filename (remove .csv extension)
        std::string const frame_str = filename.substr(0, filename.length() - 4);
        
        // Try to parse frame number
        int frame_number;
        try {
            frame_number = std::stoi(frame_str);
        } catch (std::exception const & e) {
            std::cerr << "Warning: Could not parse frame number from filename: " << filename << std::endl;
            files_skipped++;
            continue;
        }

        // Load the CSV file
        std::ifstream file(entry.path());
        if (!file.is_open()) {
            std::cerr << "Warning: Could not open file: " << entry.path() << std::endl;
            files_skipped++;
            continue;
        }

        std::vector<Point2D<float>> line_points;
        line_points.reserve(100); // Reserve space for typical line
        std::string line;
        line.reserve(256); // Reserve space for typical line length
        bool first_line = true;

        while (std::getline(file, line)) {
            // Skip header if present
            if (first_line && opts.has_header) {
                first_line = false;
                continue;
            }
            first_line = false;

            // Parse the line manually instead of using stringstream
            size_t pos = 0;
            int col_idx = 0;
            float x = 0.0f, y = 0.0f;
            bool x_found = false, y_found = false;
            
            char const delim = opts.delimiter[0];
            while (pos < line.length()) {
                size_t next_pos = line.find(delim, pos);
                if (next_pos == std::string::npos) {
                    next_pos = line.length();
                }
                
                // Parse the column value if it's one we need
                if (col_idx == opts.x_column || col_idx == opts.y_column) {
                    try {
                        float value = std::stof(line.substr(pos, next_pos - pos));
                        if (col_idx == opts.x_column) {
                            x = value;
                            x_found = true;
                        } else {
                            y = value;
                            y_found = true;
                        }
                    } catch (std::exception const & e) {
                        std::cerr << "Warning: Could not parse coordinate from line: " << line << " (file: " << filename << ")" << std::endl;
                        break;
                    }
                }
                
                pos = next_pos + 1;
                col_idx++;
                
                // Early exit if we've found both columns
                if (x_found && y_found) {
                    break;
                }
            }
            
            if (x_found && y_found) {
                line_points.push_back(Point2D<float>{x, y});
            }
        }

        file.close();

        // Add the line to the data map if we have points
        if (!line_points.empty()) {
            data_map[TimeFrameIndex(frame_number)].push_back(line_points);
            files_loaded++;
        } else {
            std::cerr << "Warning: No valid points found in file: " << filename << std::endl;
            files_skipped++;
        }
    }

    std::cout << "Multi-file CSV load complete: " << files_loaded << " files loaded";
    if (files_skipped > 0) {
        std::cout << ", " << files_skipped << " files skipped";
    }
    std::cout << std::endl;

    return data_map;
}