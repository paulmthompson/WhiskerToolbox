#include "Analog_Time_Series.hpp"

#include <iostream>
#include <fstream>
#include <sstream>
#include <vector>

AnalogTimeSeries::AnalogTimeSeries(std::map<int,float> analog_map)
{
    setData(analog_map);
}

AnalogTimeSeries::AnalogTimeSeries(std::vector<float> analog_vector)
{
    setData(analog_vector);
}

AnalogTimeSeries::AnalogTimeSeries(std::vector<float> analog_vector, std::vector<size_t> time_vector)
{
    setData(analog_vector, time_vector);
}

std::vector<float> load_analog_series_from_csv(std::string const& filename)
{

    std::string csv_line;
    std::fstream myfile;
    myfile.open (filename, std::fstream::in);

    if (!myfile.is_open()) {
        std::cout << "Error: File " << filename << " not found." << std::endl;
        return {};
    }

    std::string y_str;
    auto output = std::vector<float>{};

    while (getline(myfile, csv_line)) {

        std::stringstream ss(csv_line);

        getline(ss, y_str);

        output.push_back(std::stof(y_str));
    }

    return output;
}

void save_analog(
        std::vector<float> const & analog_series,
        std::vector<size_t> const & time_series,
        std::string const block_output)
{
    std::fstream fout;
    fout.open(block_output, std::fstream::out);

    for (size_t i = 0; i < analog_series.size(); ++i) {
        fout << time_series[i] << "," << analog_series[i] << "\n";
    }

    fout.close();
}