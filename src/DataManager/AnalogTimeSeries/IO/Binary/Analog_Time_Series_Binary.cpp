
#include "Analog_Time_Series_Binary.hpp"

#include "AnalogTimeSeries/Analog_Time_Series.hpp"
#include "AnalogTimeSeries/AnalogDataStorage.hpp"
#include "loaders/binary_loaders.hpp"

#include <algorithm>
#include <filesystem>
#include <iostream>

namespace {

MmapDataType stringToMmapDataType(std::string const& type_str) {
    if (type_str == "float32" || type_str == "float") return MmapDataType::Float32;
    if (type_str == "float64" || type_str == "double") return MmapDataType::Float64;
    if (type_str == "int8") return MmapDataType::Int8;
    if (type_str == "uint8") return MmapDataType::UInt8;
    if (type_str == "int16") return MmapDataType::Int16;
    if (type_str == "uint16") return MmapDataType::UInt16;
    if (type_str == "int32") return MmapDataType::Int32;
    if (type_str == "uint32") return MmapDataType::UInt32;
    
    std::cerr << "Warning: Unknown data type '" << type_str << "', defaulting to int16" << std::endl;
    return MmapDataType::Int16;
}

} // anonymous namespace


std::vector<std::shared_ptr<AnalogTimeSeries>> load(BinaryAnalogLoaderOptions const & opts) {

    std::vector<std::shared_ptr<AnalogTimeSeries>> analog_time_series;

    // Memory-mapped loading path
    if (opts.getUseMemoryMapped()) {
        std::filesystem::path file_path = opts.getFilename();
        if (!file_path.is_absolute()) {
            file_path = std::filesystem::path(opts.getParentDir()) / file_path;
        }
        
        // Create one memory-mapped series per channel
        for (size_t channel = 0; channel < static_cast<size_t>(opts.getNumChannels()); ++channel) {
            MmapStorageConfig config;
            config.file_path = file_path;
            config.header_size = static_cast<size_t>(opts.getHeaderSize());
            config.offset = opts.getOffset() + channel;  // Start at channel offset
            config.stride = opts.getStride() * opts.getNumChannels();  // Stride accounts for all channels
            config.data_type = stringToMmapDataType(opts.getDataType());
            config.scale_factor = opts.getScaleFactor();
            config.offset_value = opts.getOffsetValue();
            config.num_samples = opts.getNumSamples();
            
            // Create time vector for this channel
            // First, create a temporary mmap storage to get the actual sample count
            auto temp_storage = MemoryMappedAnalogDataStorage(config);
            size_t num_samples = temp_storage.size();
            
            std::vector<TimeFrameIndex> time_vector;
            for (size_t i = 0; i < num_samples; ++i) {
                time_vector.push_back(TimeFrameIndex(static_cast<int64_t>(i)));
            }
            
            // Now create the actual series with the config
            auto series = AnalogTimeSeries::createMemoryMapped(std::move(config), std::move(time_vector));
            analog_time_series.push_back(series);
        }
        
        std::cout << "Memory-mapped " << analog_time_series.size() << " channel(s)" << std::endl;
        return analog_time_series;
    }

    // Traditional in-memory loading path
    auto binary_loader_opts = Loader::BinaryAnalogOptions{.file_path = opts.getFilename(),
                                                          .header_size_bytes = static_cast<size_t>(opts.getHeaderSize()),
                                                          .num_channels = static_cast<size_t>(opts.getNumChannels())};

    if (opts.getNumChannels() > 1) {

        auto data = Loader::readBinaryFileMultiChannel<int16_t>(binary_loader_opts);

        std::cout << "Read " << data.size() << " channels" << std::endl;

        for (auto & channel: data) {

            std::vector<float> data_float(channel.begin(), channel.end());
            analog_time_series.push_back(std::make_shared<AnalogTimeSeries>(std::move(data_float),
                                                                            data_float.size()));

            // convert to float with std::transform
            /*
            std::vector<float> data_float;
            std::transform(
                    channel.begin(),
                    channel.end(),
                    std::back_inserter(data_float), [](int16_t i) { return i; });

            analog_time_series.push_back(std::make_shared<AnalogTimeSeries>(data_float, data_float.size()));
            */
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
