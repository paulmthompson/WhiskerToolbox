#ifndef DIGITAL_INTERVAL_SERIES_BINARY_HPP
#define DIGITAL_INTERVAL_SERIES_BINARY_HPP

#include "TimeFrame/interval_data.hpp"

#include <string>
#include <vector>
#include <memory>

class DigitalIntervalSeries;

/**
 * @struct BinaryIntervalLoaderOptions
 *
 * @brief Options for loading DigitalIntervalSeries data from binary files.
 *        Binary files contain packed digital data where each bit represents a channel.
 *
 * @var BinaryIntervalLoaderOptions::filepath
 * The path to the binary file to load.
 *
 * @var BinaryIntervalLoaderOptions::header_size_bytes
 * Number of bytes to skip at the beginning of the file (header). Defaults to 0.
 *
 * @var BinaryIntervalLoaderOptions::data_type_bytes
 * Size of each data sample in bytes (1, 2, 4, or 8). Defaults to 2 (uint16_t).
 *
 * @var BinaryIntervalLoaderOptions::channel
 * The channel (bit position) to extract intervals from. Defaults to 0.
 *
 * @var BinaryIntervalLoaderOptions::transition_type
 * The type of transition that defines interval boundaries:
 * - "rising": low->high starts interval, high->low ends interval
 * - "falling": high->low starts interval, low->high ends interval
 * Defaults to "rising".
 */
struct BinaryIntervalLoaderOptions {
    std::string filepath;
    size_t header_size_bytes = 0;
    int data_type_bytes = 2;  // 1=uint8, 2=uint16, 4=uint32, 8=uint64
    int channel = 0;
    std::string transition_type = "rising";
};

/**
 * @brief Load digital interval series data from binary file using specified options
 *
 * @param options Configuration options for loading
 * @return Vector of Interval objects loaded from the binary file
 */
std::vector<Interval> load(BinaryIntervalLoaderOptions const & options);

/**
 * @brief Load digital interval series data into a DigitalIntervalSeries object
 *
 * @param options Configuration options for loading
 * @return Shared pointer to DigitalIntervalSeries object
 */
std::shared_ptr<DigitalIntervalSeries> loadIntoDigitalIntervalSeries(BinaryIntervalLoaderOptions const & options);

#endif// DIGITAL_INTERVAL_SERIES_BINARY_HPP 
