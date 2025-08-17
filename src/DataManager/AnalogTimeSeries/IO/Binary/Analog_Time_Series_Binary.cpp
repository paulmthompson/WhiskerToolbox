
#include "Analog_Time_Series_Binary.hpp"

#include "AnalogTimeSeries/Analog_Time_Series.hpp"
#include "loaders/binary_loaders.hpp"

#include <iostream>


std::vector<std::shared_ptr<AnalogTimeSeries>> load(BinaryAnalogLoaderOptions & opts) {

    std::vector<std::shared_ptr<AnalogTimeSeries>> analog_time_series;

    auto binary_loader_opts = Loader::BinaryAnalogOptions{.file_path = opts.filename,
                                                          .header_size_bytes = static_cast<size_t>(opts.header_size),
                                                          .num_channels = static_cast<size_t>(opts.num_channels)};

    if (opts.num_channels > 1) {

        auto data = Loader::readBinaryFileMultiChannel<int16_t>(binary_loader_opts);

        std::cout << "Read " << data.size() << " channels" << std::endl;

        for (auto & channel: data) {
            // convert to float with std::transform
            std::vector<float> data_float;
            std::transform(
                    channel.begin(),
                    channel.end(),
                    std::back_inserter(data_float), [](int16_t i) { return i; });

            analog_time_series.push_back(std::make_shared<AnalogTimeSeries>(data_float, data_float.size()));
        }

    } else {

        auto data = Loader::readBinaryFile<int16_t>(binary_loader_opts);

        // convert to float with std::transform
        std::vector<float> data_float;
        std::transform(
                data.begin(),
                data.end(),
                std::back_inserter(data_float), [](int16_t i) { return i; });

        analog_time_series.push_back(std::make_shared<AnalogTimeSeries>(data_float, data_float.size()));
    }

    return analog_time_series;
}

