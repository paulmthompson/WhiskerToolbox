#ifndef BINARY_LOADERS_HPP
#define BINARY_LOADERS_HPP

#include <iostream>
#include <stdexcept>
#include <vector>

template <typename T>
std::vector<T> readBinaryFile(const std::string& file_path) {
    std::ifstream file(file_path, std::ios::binary);
    std::vector<T> data;
    if (file) {
        T value;
        while (file.read(reinterpret_cast<char*>(&value), sizeof(T))) {
            data.push_back(value);
        }
    }
    return data;
}

template <typename T>
std::vector<int> extractDigitalData(const std::vector<T>& data, int channel) {

    const T ttl_mask = 1 << channel;

    std::cout << "Extracting digital data from " << data.size() << " samples" << std::endl;

    std::vector<int> digital_data;
    for (auto& d : data) {
        digital_data.push_back((ttl_mask & d) > 0);
    }
    return digital_data;
}

std::vector<float> extractEvents(const std::vector<int>& digital_data, const std::string& transition) {

    // Check if transition is valid
    if (transition != "rising" && transition != "falling") {
        throw std::invalid_argument("Invalid transition type");
    }

    std::cout << "Checking " << digital_data.size() << " timestamps for events" << std::endl;
    std::vector<float> events;
    for (std::size_t i = 1; i < digital_data.size(); i++) {
        if ((digital_data[i] == 1) && (digital_data[i-1] == 0) && (transition == "rising")) {
            events.push_back(i);
        } else if ((digital_data[i] == 0) && (digital_data[i-1] == 1) && (transition == "falling")) {
            events.push_back(i);
        }
    }
    return events;
}

std::vector<std::pair<float,float>> extractIntervals(const std::vector<int>& digital_data, const std::string& transition) {

    // Check if transition is valid
    if (transition != "rising" && transition != "falling") {
        throw std::invalid_argument("Invalid transition type");
    }

    std::string start_transition = transition;
    std::string end_transition = transition == "rising" ? "falling" : "rising";

    bool in_interval = false;
    float start_time = 0;

    std::cout << "Checking " << digital_data.size() << " timestamps for intervals" << std::endl;
    std::vector<std::pair<float,float>> intervals;


    for (std::size_t i = 1; i < digital_data.size(); i++) {
        if (!in_interval &&
            ((start_transition == "rising" && digital_data[i] == 1 && digital_data[i-1] == 0) ||
             (start_transition == "falling" && digital_data[i] == 0 && digital_data[i-1] == 1))) {

            start_time = i;
            in_interval = true;

        } else if (in_interval &&
                   ((end_transition == "rising" && digital_data[i] == 1 && digital_data[i-1] == 0) ||
                    (end_transition == "falling" && digital_data[i] == 0 && digital_data[i-1] == 1))) {

            intervals.emplace_back(start_time, i);
            in_interval = false;
            
        }
    }
        return intervals;
}

#endif //BINARY_LOADERS_HPP
