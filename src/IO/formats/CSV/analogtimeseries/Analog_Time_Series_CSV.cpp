#include "Analog_Time_Series_CSV.hpp"
#include "AnalogTimeSeries/Analog_Time_Series.hpp"
#include "IO/core/AtomicWrite.hpp"

#include <cassert>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <memory>
#include <sstream>
#include <stdexcept>

std::vector<float> load_analog_series_from_csv(std::string const & filename) {

    std::string csv_line;
    std::fstream myfile;
    myfile.open(filename, std::fstream::in);

    if (!myfile.is_open()) {
        std::cerr << "Error: File " << filename << " not found." << std::endl;
        return {};
    }

    std::string y_str;
    auto output = std::vector<float>{};

    while (getline(myfile, csv_line)) {

        std::stringstream ss(csv_line);

        getline(ss, y_str);

        output.push_back(std::stof(y_str));
    }

    myfile.close();

    return output;
}

std::shared_ptr<AnalogTimeSeries> load(CSVAnalogLoaderOptions const & options) {
    std::ifstream file(options.filepath);
    if (!file.is_open()) {
        throw std::runtime_error("Error: Could not open file: " + options.filepath);
    }

    std::vector<float> data_values;
    std::vector<TimeFrameIndex> time_values;
    std::string line;

    // Skip header if present
    if (options.getHasHeader() && std::getline(file, line)) {
        // Header line consumed, continue with data
    }

    while (std::getline(file, line)) {
        if (line.empty()) continue;

        std::stringstream ss(line);
        std::string cell;
        std::vector<std::string> row;

        // Parse the line based on delimiter
        while (std::getline(ss, cell, options.getDelimiter()[0])) {
            row.push_back(cell);
        }

        if (row.empty()) continue;

        try {
            if (options.getSingleColumnFormat()) {
                // Single column format: only data, time is inferred as index
                if (!row.empty()) {
                    data_values.push_back(std::stof(row[0]));
                    time_values.emplace_back(static_cast<int64_t>(time_values.size()));// Use index as time
                }
            } else {
                // Two column format: time and data columns
                if (row.size() > static_cast<size_t>(std::max(options.getTimeColumn(), options.getDataColumn()))) {
                    time_values.emplace_back(static_cast<int64_t>(std::stof(row[static_cast<size_t>(options.getTimeColumn())])));
                    data_values.push_back(std::stof(row[static_cast<size_t>(options.getDataColumn())]));
                }
            }
        } catch (std::exception const & e) {
            std::cerr << "Warning: Could not parse line: " << line << " - " << e.what() << std::endl;
            continue;
        }
    }

    file.close();

    if (data_values.empty()) {
        throw std::runtime_error("Error: No valid data found in file: " + options.filepath);
    }

    return std::make_shared<AnalogTimeSeries>(data_values, time_values);
}

bool save(AnalogTimeSeries const * analog_data,
          CSVAnalogSaverOptions const & opts) {
    assert(analog_data && "save: analog_data must not be null");

    auto const target_path = std::filesystem::path(opts.parent_dir) / opts.filename;

    bool const ok = atomicWriteFile(target_path, [&](std::ostream & out) {
        if (opts.save_header) {
            out << opts.header << opts.line_delim;
        }

        out << std::fixed << std::setprecision(opts.precision);

        for (auto const & sample: analog_data->getAllSamples()) {
            out << sample.time_frame_index.getValue()
                << opts.delimiter
                << sample.value()
                << opts.line_delim;
        }
        return out.good();
    });

    if (ok) {
        std::cout << "Successfully saved analog data to " << target_path << std::endl;
    }
    return ok;
}
