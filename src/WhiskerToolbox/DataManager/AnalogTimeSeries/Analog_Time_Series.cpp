#include "Analog_Time_Series.hpp"

#include <fstream>
#include <sstream>
#include <vector>

AnalogTimeSeries::AnalogTimeSeries(std::vector<float> analog_vector)
{
    _data = analog_vector;
}


std::vector<float> const & AnalogTimeSeries::getAnalogTimeSeries() const
{
    return _data;
}

std::vector<float> load_analog_series_from_csv(std::string const& filename)
{

    std::string csv_line;
    std::fstream myfile;
    myfile.open (filename, std::fstream::in);

    if (!myfile.is_open()) {
        std::out << "Error: File " << filename << " not found." << std::endl;
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
