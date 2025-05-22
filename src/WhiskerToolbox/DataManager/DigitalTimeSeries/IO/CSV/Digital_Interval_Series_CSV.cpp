#include "Digital_Interval_Series_CSV.hpp"

#include "DigitalTimeSeries/Digital_Interval_Series.hpp" // Required for DigitalIntervalSeries

#include <cmath>   // For std::round, though intervals are int64_t, good to keep if format changes
#include <fstream>
#include <sstream>
#include <filesystem> // Required for std::filesystem
#include <iostream>   // Required for std::cout, std::cerr

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

    int64_t start, end;
    auto output = std::vector<Interval>();
    while (getline(myfile, csv_line)) {
        std::stringstream ss(csv_line);
        ss >> start >> delimiter >> end;
        output.emplace_back(Interval{start, end});
    }

    return output;
}

void save_digital_interval_series_to_csv(
        DigitalIntervalSeries const * interval_data,
        CSVIntervalSaverOptions const & opts) {
    
    if (!interval_data) {
        std::cerr << "Error: DigitalIntervalSeries data is null. Cannot save." << std::endl;
        return;
    }

    if (!opts.parent_dir.empty() && !std::filesystem::exists(opts.parent_dir)) {
        try {
            std::filesystem::create_directories(opts.parent_dir);
            std::cout << "Created directory: " << opts.parent_dir << std::endl;
        } catch (std::filesystem::filesystem_error const& e) {
            std::cerr << "Error creating directory " << opts.parent_dir << ": " << e.what() << std::endl;
            return;
        }
    }

    std::string full_path = opts.parent_dir.empty() ? opts.filename : (std::filesystem::path(opts.parent_dir) / opts.filename).string();

    std::ofstream fout;
    fout.open(full_path, std::ios_base::out | std::ios_base::trunc);

    if (!fout.is_open()) {
        std::cerr << "Error: Could not open file for writing: " << full_path << std::endl;
        return;
    }

    if (opts.save_header && !opts.header.empty()) {
        fout << opts.header << opts.line_delim;
    }

    std::vector<Interval> const & intervals = interval_data->getDigitalIntervalSeries();

    for (auto const & interval : intervals) {
        // Using interval.start and interval.end directly as they are int64_t
        fout << interval.start << opts.delimiter << interval.end << opts.line_delim;
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

