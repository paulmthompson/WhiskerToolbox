#ifndef BINARY_LOADERS_HPP
#define BINARY_LOADERS_HPP

#include "TimeFrame/TimeFrame.hpp"

// Uncomment to enable profiling timers in this file
// #define BINARY_LOADERS_ENABLE_PROFILING

#ifdef BINARY_LOADERS_ENABLE_PROFILING
#include <chrono>
#endif
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <type_traits>
#include <vector>

namespace Loader {

struct BinaryAnalogOptions {
    std::string file_path;
    size_t header_size_bytes = 0;
    size_t num_channels = 1;
};

/**
 * @brief readBinaryFile
 *
 * Reads a binary file and returns the data as a vector of type T
 *
 * @param file_path Path to the binary file
 * @param header_size_bytes (optional) Number of bytes to skip at the beginning of the file
 * @return std::vector<T> Vector of data read from the file
 */
template<typename T>
inline std::vector<T> readBinaryFile(BinaryAnalogOptions const & options) {

#ifdef BINARY_LOADERS_ENABLE_PROFILING
    auto t1 = std::chrono::steady_clock::now();
#endif

    std::ifstream file(options.file_path, std::ios::binary | std::ios::ate);// seeks to end

    if (!file) {
        std::cout << "Cannot open file: " << options.file_path << std::endl;
        return std::vector<T>();
    }

    std::streampos const file_size = file.tellg();

    size_t const data_size_bytes = static_cast<size_t>(file_size) - options.header_size_bytes;
    size_t num_samples = data_size_bytes / sizeof(T);

    std::vector<T> data(num_samples);

    file.seekg(static_cast<std::ios::off_type>(options.header_size_bytes), std::ios::beg);
    file.read(reinterpret_cast<char *>(data.data()), static_cast<std::streamsize>(data_size_bytes));

#ifdef BINARY_LOADERS_ENABLE_PROFILING
    auto t2 = std::chrono::steady_clock::now();
    auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(t2 - t1);
    std::cout << "Total time to load " << options.file_path << ": " << elapsed.count() << std::endl;
#endif

    return data;
}

template<typename T>
inline std::vector<std::vector<T>> readBinaryFileMultiChannel(BinaryAnalogOptions const & options) {

#ifdef BINARY_LOADERS_ENABLE_PROFILING
    auto t1 = std::chrono::steady_clock::now();
#endif

    if (options.num_channels <= 0) {
        std::cout << "Channels cannot be less than 1" << std::endl;
        return std::vector<std::vector<T>>();
    }

    std::ifstream file(options.file_path, std::ios::binary | std::ios::ate);// Opens file at end

    if (!file) {
        std::cout << "Cannot open file: " << options.file_path << std::endl;
        return std::vector<std::vector<T>>();
    }

    std::streampos const file_size_bytes = file.tellg();
    if (file_size_bytes < static_cast<std::streamoff>(options.header_size_bytes)) {
        std::cout << "File size is smaller than header size" << std::endl;
        return std::vector<std::vector<T>>();
    }

    size_t const data_size_bytes = static_cast<size_t>(file_size_bytes) - options.header_size_bytes;
    size_t bytes_per_sample_set = options.num_channels * sizeof(T);

    if (data_size_bytes % bytes_per_sample_set != 0) {
        std::cout << "Warning: The bytes in data is not a multiple of number of channels" << std::endl;
    }

    size_t num_samples_per_channel = data_size_bytes / bytes_per_sample_set;

    std::vector<std::vector<T>> data;
    data.resize(options.num_channels);

    for (size_t i = 0; i < options.num_channels; i++) {
        data[i].resize(num_samples_per_channel);
    }

    file.seekg(static_cast<std::ios::off_type>(options.header_size_bytes), std::ios::beg);

    // Read in chunks for better I/O performance
    // Chunk size of 10000 time samples balances memory usage and syscall overhead
    // For 32 channels of int16, this is 640KB per chunk - fits comfortably in L2/L3 cache
    constexpr size_t CHUNK_TIME_SAMPLES = 10000;
    std::vector<T> chunk_buffer(CHUNK_TIME_SAMPLES * options.num_channels);

    size_t time_offset = 0;
    while (time_offset < num_samples_per_channel) {
        size_t const chunk_size = std::min(CHUNK_TIME_SAMPLES, num_samples_per_channel - time_offset);
        size_t const bytes_to_read = chunk_size * options.num_channels * sizeof(T);

        file.read(reinterpret_cast<char *>(chunk_buffer.data()), static_cast<std::streamsize>(bytes_to_read));

        // Distribute chunk data to per-channel vectors
        for (size_t t = 0; t < chunk_size; ++t) {
            for (size_t ch = 0; ch < options.num_channels; ++ch) {
                data[ch][time_offset + t] = chunk_buffer[t * options.num_channels + ch];
            }
        }

        time_offset += chunk_size;
    }

#ifdef BINARY_LOADERS_ENABLE_PROFILING
    auto t2 = std::chrono::steady_clock::now();
    auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(t2 - t1);
    std::cout << "Total time to load " << elapsed.count() << std::endl;
#endif

    return data;
}


/**
 * @brief extractDigitalData
 *
 * This reads a vector of unsigned ints where
 * each bit represents a digital channel and extracts
 * the data for a specific channel (1 or 0)
 *
 * @param data Vector of data
 * @param channel Channel to extract
 * @return std::vector<int> Vector of digital data
 */
template<typename T, typename std::enable_if<std::is_integral<T>::value && std::is_unsigned<T>::value, int>::type = 0>
std::vector<int> extractDigitalData(std::vector<T> const & data, int channel) {

    T const ttl_mask = 1 << channel;

    std::cout << "Extracting digital data from " << data.size() << " samples" << std::endl;

    std::vector<int> digital_data;
    for (auto & d: data) {
        digital_data.push_back((ttl_mask & d) > 0);
    }
    return digital_data;
}

inline std::vector<TimeFrameIndex> extractEvents(std::vector<int> const & digital_data, std::string const & transition) {

    // Check if transition is valid
    if (transition != "rising" && transition != "falling") {
        throw std::invalid_argument("Invalid transition type");
    }

    std::cout << "Checking " << digital_data.size() << " timestamps for events" << std::endl;
    std::vector<TimeFrameIndex> events;
    for (std::size_t i = 1; i < digital_data.size(); i++) {
        if (
                ((digital_data[i] == 1) && (digital_data[i - 1] == 0) && (transition == "rising")) ||
                ((digital_data[i] == 0) && (digital_data[i - 1] == 1) && (transition == "falling"))) {
            events.push_back(TimeFrameIndex(static_cast<int64_t>(i)));
        }
    }
    return events;
}

inline std::vector<std::pair<float, float>> extractIntervals(std::vector<int> const & digital_data, std::string const & transition) {

    // Check if transition is valid
    if (transition != "rising" && transition != "falling") {
        throw std::invalid_argument("Invalid transition type");
    }

    std::string const & start_transition = transition;
    std::string const end_transition = transition == "rising" ? "falling" : "rising";

    bool in_interval = false;
    float start_time = 0;

    std::cout << "Checking " << digital_data.size() << " timestamps for intervals" << std::endl;
    std::vector<std::pair<float, float>> intervals;


    for (std::size_t i = 1; i < digital_data.size(); i++) {
        if (!in_interval &&
            ((start_transition == "rising" && digital_data[i] == 1 && digital_data[i - 1] == 0) ||
             (start_transition == "falling" && digital_data[i] == 0 && digital_data[i - 1] == 1))) {

            start_time = static_cast<float>(i);
            in_interval = true;

        } else if (in_interval &&
                   ((end_transition == "rising" && digital_data[i] == 1 && digital_data[i - 1] == 0) ||
                    (end_transition == "falling" && digital_data[i] == 0 && digital_data[i - 1] == 1))) {

            intervals.emplace_back(start_time, i);
            in_interval = false;
        }
    }

    return intervals;
}

}// namespace Loader

#endif//BINARY_LOADERS_HPP
