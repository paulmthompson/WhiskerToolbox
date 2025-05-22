#include "Analog_Time_Series_CSV.hpp"
#include "AnalogTimeSeries/Analog_Time_Series.hpp"

#include <fstream>
#include <iostream>
#include <sstream>
#include <filesystem> // Required for std::filesystem
#include <iomanip>    // Required for std::fixed and std::setprecision

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

void save_analog_series_to_csv(
        AnalogTimeSeries * analog_data,
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
        fout << analog_data->getTimeAtIndex(i) 
             << opts.delimiter 
             << analog_data->getDataAtIndex(i) 
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
