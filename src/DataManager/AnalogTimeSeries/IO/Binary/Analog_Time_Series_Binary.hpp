#ifndef ANALOG_TIME_SERIES_LOADER_HPP
#define ANALOG_TIME_SERIES_LOADER_HPP

#include <memory>
#include <string>
#include <vector>

class AnalogTimeSeries;

struct BinaryAnalogLoaderOptions {
    std::string filename;
    std::string parent_dir = ".";
    int header_size = 0;
    int num_channels = 1;
};

std::vector<std::shared_ptr<AnalogTimeSeries>> load(BinaryAnalogLoaderOptions const & opts);

#endif// ANALOG_TIME_SERIES_LOADER_HPP
