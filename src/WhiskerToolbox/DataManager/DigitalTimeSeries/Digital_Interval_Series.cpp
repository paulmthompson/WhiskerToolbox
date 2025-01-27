#include "Digital_Interval_Series.hpp"

#include <algorithm>
#include <fstream>
#include <sstream>
#include <vector>
#include <utility>

DigitalIntervalSeries::DigitalIntervalSeries(std::vector<std::pair<float, float>> digital_vector)
{
    _data = digital_vector;
    _sortData();
}

std::vector<std::pair<float, float>> const & DigitalIntervalSeries::getDigitalIntervalSeries() const
{
    return _data;
}

bool DigitalIntervalSeries::isEventAtTime(int time) const
{
    for (auto event : _data) {
        if (time >= event.first && time <= event.second) {
            return true;
        }
    }
    return false;
}

void DigitalIntervalSeries::createIntervalsFromBool(std::vector<uint8_t> const& bool_vector)
{
    bool in_interval = false;
    int start = 0;
    for (int i = 0; i < bool_vector.size(); ++i) {
        if (bool_vector[i] && !in_interval) {
            start = i;
            in_interval = true;
        } else if (!bool_vector[i] && in_interval) {
            _data.push_back(std::make_pair(start, i - 1));
            in_interval = false;
        }
    }
    if (in_interval) {
        _data.push_back(std::make_pair(start, bool_vector.size() - 1));
    }

    _sortData();
    notifyObservers();
}

void DigitalIntervalSeries::setEventAtTime(int time, bool event)
{
    if (!event)
    {
        for (auto it = _data.begin(); it != _data.end(); ++it) {
            if (time >= it->first && time <= it->second) {
                if (time == it->first && time == it->second) {
                    _data.erase(it);
                } else if (time == it->first) {
                    it->first = time + 1;
                } else if (time == it->second) {
                    it->second = time - 1;
                } else {
                    auto preceding_event = std::make_pair(it->first, time - 1);
                    auto following_event = std::make_pair(time + 1, it->second);
                    _data.erase(it);
                    _data.push_back(preceding_event);
                    _data.push_back(following_event);

                    _sortData();
                    notifyObservers();
                    return;
                }
            }
        }
    } else {
        addEvent(time, time);
    }
    _sortData();
    notifyObservers();
}

void DigitalIntervalSeries::_sortData()
{
    std::sort(_data.begin(), _data.end());
}

int find_closest_preceding_event(DigitalIntervalSeries * digital_series, int time)
{
    auto const events = digital_series->getDigitalIntervalSeries();

    // Check if sorted
    for (int i = 1; i < events.size(); ++i) {
        if (events[i].first < events[i-1].first) {
            throw std::runtime_error("DigitalIntervalSeries is not sorted");
        }
    }
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