#include "Line_Data_CSV.hpp"

#include "Lines/Line_Data.hpp"
#include "utils/string_manip.hpp"

#include <chrono>
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
    for (auto const & frame_and_line: line_data->GetAllLinesAsRange()) {
        for (auto const & line: frame_and_line.lines) {
            std::ostringstream x_values;
            std::ostringstream y_values;

            for (auto const & point: line) {
                x_values << std::fixed << std::setprecision(opts.precision) << point.x << opts.delimiter;
                y_values << std::fixed << std::setprecision(opts.precision) << point.y << opts.delimiter;
            }

            // Remove the trailing delimiter
            std::string x_str = x_values.str();
            std::string y_str = y_values.str();
            if (!x_str.empty()) x_str.pop_back();
            if (!y_str.empty()) y_str.pop_back();

            file << frame_and_line.time << ",\"" << x_str << "\",\"" << y_str << "\"\n";
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
    for (auto const & frame_and_line: line_data->GetAllLinesAsRange()) {
        // Only save if there are lines at this timestamp
        if (frame_and_line.lines.empty()) {
            files_skipped++;
            continue;
        }

        // Only save the first line (index 0) as documented
        Line2D const & first_line = frame_and_line.lines[0];
        
        // Generate filename with zero-padded frame number
        std::string const padded_frame = pad_frame_id(frame_and_line.time, opts.frame_id_padding);
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

    std::stringstream ss(str);
    std::string item;

    char delim_char = delimiter.empty() ? ',' : delimiter[0];
    while (std::getline(ss, item, delim_char)) {
        result.push_back(std::stof(item));
    }
    return result;
}

std::map<int, std::vector<Line2D>> load(CSVSingleFileLineLoaderOptions const & opts) {
    auto t1 = std::chrono::high_resolution_clock::now();
    std::map<int, std::vector<Line2D>> data_map;
    std::ifstream file(opts.filepath);
    if (!file.is_open()) {
        throw std::runtime_error("Could not open file: " + opts.filepath);
    }

    std::string line;
    int loaded_lines = 0;
    
    while (std::getline(file, line)) {
        std::istringstream ss(line);
        std::string frame_num_str, x_str, y_str;

        // Get frame number (first column)
        std::getline(ss, frame_num_str, opts.delimiter[0]);

        // Skip header if present
        if (opts.has_header && frame_num_str == opts.header_identifier) {
            continue;
        }

        // Get X coordinates (second column, enclosed in quotes)
        std::getline(ss, x_str, '"');
        std::getline(ss, x_str, '"');

        // Get Y coordinates (third column, enclosed in quotes)
        std::getline(ss, y_str, '"');
        std::getline(ss, y_str, '"');

        int const frame_num = std::stoi(frame_num_str);

        std::vector<float> const x_values = parse_string_to_float_vector(x_str, opts.coordinate_delimiter);
        std::vector<float> const y_values = parse_string_to_float_vector(y_str, opts.coordinate_delimiter);

        if (x_values.size() != y_values.size()) {
            std::cerr << "Mismatched x and y values at frame: " << frame_num << std::endl;
            continue;
        }

        if (data_map.find(frame_num) == data_map.end()) {
            data_map[frame_num] = std::vector<Line2D>();
        }

        data_map[frame_num].emplace_back(create_line(x_values, y_values));
        loaded_lines += 1;
    }

    file.close();
    auto t2 = std::chrono::high_resolution_clock::now();

    auto duration = std::chrono::duration<double>(t2 - t1).count();
    std::cout << "Loaded " << loaded_lines << " lines from " << opts.filepath << " in " << duration << "s" << std::endl;
    return data_map;
}

std::map<int, std::vector<Line2D>> load_line_csv(std::string const & filepath) {
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

std::map<int, std::vector<Line2D>> load(CSVMultiFileLineLoaderOptions const & opts) {
    std::map<int, std::vector<Line2D>> data_map;
    
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
        std::string line;
        bool first_line = true;

        while (std::getline(file, line)) {
            // Skip header if present
            if (first_line && opts.has_header) {
                first_line = false;
                continue;
            }
            first_line = false;

            // Parse the line
            std::stringstream ss(line);
            std::vector<std::string> columns;
            std::string column;

            // Split by delimiter
            while (std::getline(ss, column, opts.delimiter[0])) {
                columns.push_back(column);
            }

            // Check if we have enough columns
            int max_column = std::max(opts.x_column, opts.y_column);
            if (static_cast<int>(columns.size()) <= max_column) {
                std::cerr << "Warning: Not enough columns in line: " << line << " (file: " << filename << ")" << std::endl;
                continue;
            }

            // Parse X and Y coordinates
            try {
                float x = std::stof(columns[opts.x_column]);
                float y = std::stof(columns[opts.y_column]);
                line_points.push_back(Point2D<float>{x, y});
            } catch (std::exception const & e) {
                std::cerr << "Warning: Could not parse coordinates from line: " << line << " (file: " << filename << ")" << std::endl;
                continue;
            }
        }

        file.close();

        // Add the line to the data map if we have points
        if (!line_points.empty()) {
            if (data_map.find(frame_number) == data_map.end()) {
                data_map[frame_number] = std::vector<Line2D>();
            }
            data_map[frame_number].push_back(line_points);
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
