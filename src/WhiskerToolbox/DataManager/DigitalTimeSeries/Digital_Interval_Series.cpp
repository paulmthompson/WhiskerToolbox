#include "Digital_Interval_Series.hpp"

#include <fstream>
#include <sstream>
#include <vector>
#include <utility>

DigitalIntervalSeries::DigitalIntervalSeries(std::vector<std::pair<float, float>> digital_vector)
{
    _data = digital_vector;
}

std::vector<std::pair<float, float>> const & DigitalIntervalSeries::getDigitalIntervalSeries() const
{
    return _data;
}

std::vector<std::pair<float, float>> load_digital_series_from_csv(std::string const& filename){
    std::string csv_line;

    std::fstream myfile;
    myfile.open (filename, std::fstream::in);

    float start, end;
    auto output = std::vector<std::pair<float, float>>();
    while (getline(myfile, csv_line)) {
        std::stringstream ss(csv_line);
        ss >> start >> end;
        output.emplace_back(start, end);
    }

    return output;
}