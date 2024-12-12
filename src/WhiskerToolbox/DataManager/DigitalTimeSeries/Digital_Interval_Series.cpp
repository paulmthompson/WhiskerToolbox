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

int find_closest_preceding_event(DigitalIntervalSeries * digital_series, int time)
{
    auto const events = digital_series->getDigitalIntervalSeries();
    int closest_index = -1;
    for (int i = 0; i < events.size(); ++i) {
        if (events[i].first <= time) {
            closest_index = i;
            if (time <= events[i].second) {
                return i;
            }
        } else {
            break;
        }
    }
    return closest_index;
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
