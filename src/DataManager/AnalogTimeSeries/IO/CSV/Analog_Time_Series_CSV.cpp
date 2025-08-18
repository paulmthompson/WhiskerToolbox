#include "Analog_Time_Series_CSV.hpp"
#include "AnalogTimeSeries/Analog_Time_Series.hpp"

#include <fstream>
#include <iostream>
#include <sstream>
#include <filesystem> // std::filesystem
#include <iomanip>    // std::fixed, std::setprecision
#include <memory>
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
    if (options.has_header && std::getline(file, line)) {
        // Header line consumed, continue with data
    }

    while (std::getline(file, line)) {
        if (line.empty()) continue;
        
        std::stringstream ss(line);
        std::string cell;
        std::vector<std::string> row;
        
        // Parse the line based on delimiter
        while (std::getline(ss, cell, options.delimiter[0])) {
            row.push_back(cell);
        }
        
        if (row.empty()) continue;
        
        try {
            if (options.single_column_format) {
                // Single column format: only data, time is inferred as index
                if (!row.empty()) {
                    data_values.push_back(std::stof(row[0]));
                    time_values.push_back(TimeFrameIndex(time_values.size())); // Use index as time
                }
            } else {
                // Two column format: time and data columns
                if (row.size() > std::max(options.time_column, options.data_column)) {
                    time_values.push_back(TimeFrameIndex(static_cast<int64_t>(std::stof(row[options.time_column]))));
                    data_values.push_back(std::stof(row[options.data_column]));
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

void save(AnalogTimeSeries * analog_data,
          CSVAnalogSaverOptions & opts) {

    //Check if directory exists
    if (!std::filesystem::exists(opts.parent_dir)) {
        std::filesystem::create_directories(opts.parent_dir);
        std::cout << "Created directory: " << opts.parent_dir << std::endl;
    }

    std::string filename = opts.parent_dir + "/" + opts.filename;
    std::fstream fout;

    fout.open(filename, std::fstream::out);
    if (!fout.is_open()) {
        std::cerr << "Failed to open file for saving: " << filename << std::endl;
        return;
    }

    if (opts.save_header) {
        fout << opts.header << opts.line_delim;
    }
    // Set precision for floating point numbers
    fout << std::fixed << std::setprecision(opts.precision);

    size_t num_samples = analog_data->getNumSamples();
    for (size_t i = 0; i < num_samples; ++i) {
        fout << analog_data->getTimeFrameIndexAtDataArrayIndex(DataArrayIndex(i)).getValue() 
             << opts.delimiter 
             << analog_data->getDataAtDataArrayIndex(DataArrayIndex(i)) 
             << opts.line_delim;
        if (fout.fail()) {
            std::cerr << "Error: Failed while writing data to file: " << filename << std::endl;
            fout.close();
            return;
        }
    }

    fout.close();
    if (fout.fail()) { // Check for errors after closing (e.g., flush errors)
         std::cerr << "Error: Failed to properly close file: " << filename << std::endl;
    } else {
        std::cout << "Successfully saved analog data to " << filename << std::endl;
    }
}
