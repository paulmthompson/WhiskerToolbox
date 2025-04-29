#include "Analog_Time_Series.hpp"

#include <fstream>
#include <iostream>
#include <sstream>
#include <vector>

AnalogTimeSeries::AnalogTimeSeries(std::map<int, float> analog_map) {
    setData(std::move(analog_map));
}

AnalogTimeSeries::AnalogTimeSeries(std::vector<float> analog_vector) {
    setData(std::move(analog_vector));
}

AnalogTimeSeries::AnalogTimeSeries(std::vector<float> analog_vector, std::vector<size_t> time_vector) {
    setData(std::move(analog_vector), std::move(time_vector));
}



void save_analog(
        std::vector<float> const & analog_series,
        std::vector<size_t> const & time_series,
        std::string const & block_output) {
    std::fstream fout;
    fout.open(block_output, std::fstream::out);

    for (size_t i = 0; i < analog_series.size(); ++i) {
        fout << time_series[i] << "," << analog_series[i] << "\n";
    }

    fout.close();
}
