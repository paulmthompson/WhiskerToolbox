#include "Line_Data_CSV.hpp"

#include "IO/core/AtomicWrite.hpp"
#include "Lines/Line_Data.hpp"
#include "CoreUtilities/string_manip.hpp"

#include <cassert>
#include <chrono>
#include <cstddef>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <stdexcept>
#include <charconv>

void save_line_as_csv(Line2D const & line, std::string const & filename, int const point_precision) {
    std::fstream myfile;
    myfile.open(filename, std::fstream::out);

    myfile << std::fixed << std::setprecision(point_precision);
    for (auto & point: line) {
        myfile << point.x << "," << point.y << "\n";
    }

    myfile.close();
}

bool save(
        LineData const * line_data,
        CSVSingleFileLineSaverOptions const & opts) {
    assert(line_data && "save: line_data must not be null");

    auto const target_path = std::filesystem::path(opts.parent_dir) / opts.filename;

    bool const ok = atomicWriteFile(target_path, [&](std::ostream & out) {
        // Write the header
        if (opts.save_header) {
            out << opts.header << "\n";
        }

        // Write the data
        for (auto const & [time, entity_id, line]: line_data->flattened_data()) {
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

            out << time.getValue() << ",\"" << x_str << "\",\"" << y_str << "\"\n";
        }
        return out.good();
    });

    if (ok) {
        std::cout << "Successfully saved line data to " << target_path << std::endl;
    }
    return ok;
}

bool save(
        LineData const * line_data,
        CSVMultiFileLineSaverOptions const & opts) {
    assert(line_data && "save: line_data must not be null");

    // Ensure parent directory exists
    if (!std::filesystem::exists(opts.parent_dir)) {
        std::filesystem::create_directories(opts.parent_dir);
        std::cout << "Created directory: " << opts.parent_dir << std::endl;
    }

    int files_saved = 0;
    int files_skipped = 0;
    bool any_failure = false;

    // Iterate through all timestamps with data
    for (auto const & [time, entity_id, line]: line_data->flattened_data()) {

        // Generate filename with zero-padded frame number
        std::string const padded_frame = pad_frame_id(static_cast<int>(time.getValue()), opts.frame_id_padding);
        auto const target_path = std::filesystem::path(opts.parent_dir) / (padded_frame + ".csv");

        // Check if file exists and handle according to overwrite setting
        bool const file_exists = std::filesystem::exists(target_path);
        if (file_exists && !opts.overwrite_existing) {
            files_skipped++;
            continue;
        }

        bool const ok = atomicWriteFile(target_path, [&](std::ostream & out) {
            // Write the header if requested
            if (opts.save_header) {
                out << opts.header << opts.line_delim;
            }

            // Write X and Y coordinates in separate columns
            out << std::fixed << std::setprecision(opts.precision);
            for (auto const & point: line) {
                out << point.x << opts.delimiter << point.y << opts.line_delim;
            }
            return out.good();
        });

        if (ok) {
            files_saved++;
        } else {
            files_skipped++;
            any_failure = true;
        }
    }

    std::cout << "Multi-file CSV save complete: " << files_saved << " files saved";
    if (files_skipped > 0) {
        std::cout << ", " << files_skipped << " timestamps skipped (no lines or file errors)";
    }
    std::cout << std::endl;

    return !any_failure;
}

std::vector<float> parse_string_to_float_vector(std::string_view str, std::string const & delimiter) {
    std::vector<float> result;

    // Reserve space to avoid reallocations - estimate based on string length
    // Assume average of 6 chars per number (including delimiter)
    result.reserve(str.length() / 6 + 1);

    char const delim_char = delimiter.empty() ? ',' : delimiter[0];
    char const * start = str.data();
    char const * end = start + str.length();

    // Use strtof directly instead of creating string_view or substring
    while (start < end) {
        float value = 0.0f;
        auto [parse_end, ec] = std::from_chars(start, end, value);
        if (ec != std::errc()) {
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

std::vector<std::pair<TimeFrameIndex, Line2D>> load(CSVSingleFileLineLoaderOptions const & opts) {
    auto t1 = std::chrono::high_resolution_clock::now();
    std::vector<std::pair<TimeFrameIndex, Line2D>> frame_data;
    std::ifstream file(opts.filepath);
    if (!file.is_open()) {
        throw std::runtime_error("Could not open file: " + opts.filepath);
    }

    // Use larger buffer for better I/O performance
    constexpr auto buffer_size = static_cast<size_t const>(1024 * 1024);// 1MB buffer
    std::vector<char> buffer(buffer_size);
    file.rdbuf()->pubsetbuf(buffer.data(), static_cast<std::streamsize>(buffer_size));

    std::string line;
    line.reserve(4096);// Reserve space for typical line length
    int loaded_lines = 0;
    bool is_first_line = true;

    // Get options with defaults via helper methods
    std::string const delimiter = opts.getDelimiter();
    std::string const coordinate_delimiter = opts.getCoordinateDelimiter();
    bool const has_header = opts.getHasHeader();
    std::string const header_identifier = opts.getHeaderIdentifier();

    while (std::getline(file, line)) {
        // Skip header if present
        if (is_first_line && has_header) {
            is_first_line = false;
            if (line.find(header_identifier) != std::string::npos) {
                continue;
            }
        }
        is_first_line = false;

        // Parse line manually to avoid multiple string copies
        size_t pos = 0;
        size_t const comma_pos = line.find(delimiter[0], pos);
        if (comma_pos == std::string::npos) {
            continue;
        }

        // Parse frame number directly using from_chars to avoid substring copy
        int frame_num = 0;
        std::from_chars(line.data() + pos, line.data() + comma_pos, frame_num);
        pos = comma_pos + 1;

        // Find first quote for X coordinates
        size_t const quote1 = line.find('"', pos);
        if (quote1 == std::string::npos) {
            continue;
        }
        size_t const quote2 = line.find('"', quote1 + 1);
        if (quote2 == std::string::npos) {
            continue;
        }

        // Extract X coordinates string (avoiding copy by using pointer arithmetic)
        size_t const x_start = quote1 + 1;
        size_t const x_len = quote2 - x_start;

        // Find quotes for Y coordinates
        size_t const quote3 = line.find('"', quote2 + 1);
        if (quote3 == std::string::npos) {
            continue;
        }
        size_t const quote4 = line.find('"', quote3 + 1);
        if (quote4 == std::string::npos) {
            continue;
        }

        size_t const y_start = quote3 + 1;
        size_t const y_len = quote4 - y_start;

        // Parse coordinates directly using string_view to avoid copies
        std::string_view const x_str(line.data() + x_start, x_len);
        std::string_view const y_str(line.data() + y_start, y_len);

        std::vector<float> const x_values = parse_string_to_float_vector(x_str, coordinate_delimiter);
        std::vector<float> const y_values = parse_string_to_float_vector(y_str, coordinate_delimiter);

        if (x_values.size() != y_values.size()) {
            std::cerr << "Mismatched x and y values at frame: " << frame_num << std::endl;
            continue;
        }

        frame_data.emplace_back(TimeFrameIndex(frame_num), create_line(x_values, y_values));
        loaded_lines += 1;
    }

    file.close();
    auto t2 = std::chrono::high_resolution_clock::now();

    auto duration = std::chrono::duration<double>(t2 - t1).count();
    std::cout << "Loaded " << loaded_lines << " lines from " << opts.filepath << " in " << duration << "s" << std::endl;
    return frame_data;
}

std::vector<std::pair<TimeFrameIndex, Line2D>> load_line_csv(std::string const & filepath) {
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

    std::ifstream myfile(filename);
    if (!myfile.is_open()) return line_output;

    while (std::getline(myfile, csv_line)) {
        size_t const comma_pos = csv_line.find(',');
        if (comma_pos == std::string::npos) continue;

        float x = 0.0f;
        float y = 0.0f;
        std::from_chars(csv_line.data(), csv_line.data() + comma_pos, x);
        std::from_chars(csv_line.data() + comma_pos + 1, csv_line.data() + csv_line.length(), y);

        line_output.push_back(Point2D<float>{x, y});
    }

    return line_output;
}

std::vector<std::pair<TimeFrameIndex, Line2D>> load(CSVMultiFileLineLoaderOptions const & opts) {
    std::vector<std::pair<TimeFrameIndex, Line2D>> frame_data;

    // Check if directory exists
    if (!std::filesystem::exists(opts.parent_dir)) {
        std::cerr << "Error: Directory does not exist: " << opts.parent_dir << std::endl;
        return frame_data;
    }

    int files_loaded = 0;
    int files_skipped = 0;

    // Get options with defaults via helper methods
    std::string const delimiter = opts.getDelimiter();
    int const x_column = opts.getXColumn();
    int const y_column = opts.getYColumn();
    bool const has_header = opts.getHasHeader();

    // Iterate through all files in the directory
    for (auto const & entry: std::filesystem::directory_iterator(opts.parent_dir)) {
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
        int frame_number = 0;
        auto [ptr, ec] = std::from_chars(frame_str.data(), frame_str.data() + frame_str.length(), frame_number);
        if (ec != std::errc() || ptr != frame_str.data() + frame_str.length()) {
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
        line_points.reserve(100);// Reserve space for typical line
        std::string line;
        line.reserve(256);// Reserve space for typical line length
        bool first_line = true;

        while (std::getline(file, line)) {
            // Skip header if present
            if (first_line && has_header) {
                first_line = false;
                continue;
            }
            first_line = false;

            // Parse the line manually instead of using stringstream
            size_t pos = 0;
            int col_idx = 0;
            float x = 0.0f, y = 0.0f;
            bool x_found = false, y_found = false;

            char const delim = delimiter[0];
            while (pos < line.length()) {
                size_t next_pos = line.find(delim, pos);
                if (next_pos == std::string::npos) {
                    next_pos = line.length();
                }

                // Parse the column value if it's one we need
                if (col_idx == x_column || col_idx == y_column) {
                    float value = 0.0f;
                    auto [parse_end_col, ec_col] = std::from_chars(line.data() + pos, line.data() + next_pos, value);
                    if (ec_col == std::errc()) {
                        if (col_idx == x_column) {
                            x = value;
                            x_found = true;
                        } else {
                            y = value;
                            y_found = true;
                        }
                    } else {
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
                line_points.emplace_back(x, y);
            }
        }

        file.close();

        // Add the line to the data map if we have points
        if (!line_points.empty()) {
            frame_data.emplace_back(TimeFrameIndex(frame_number), Line2D{line_points});
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

    return frame_data;
}