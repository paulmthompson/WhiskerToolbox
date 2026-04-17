#include "Analog_Time_Series_Binary.hpp"

#include "formats/Binary/common/binary_loaders.hpp"

#include "AnalogTimeSeries/Analog_Time_Series.hpp"
#include "AnalogTimeSeries/storage/AnalogDataStorage.hpp"
#include "AnalogTimeSeries/storage/BlockCachedMmapAnalogStorage.hpp"
#include "AnalogTimeSeries/storage/MemoryMappedAnalogDataStorage.hpp"
#include "AnalogTimeSeries/storage/SharedMmapBlockCache.hpp"
#include "AnalogTimeSeries/storage/TensorColumnAnalogStorage.hpp"
#include "Tensors/TensorData.hpp"
#include "Tensors/storage/MmapTensorStorage.hpp"
#include "Tensors/storage/TensorStorageWrapper.hpp"
#include "TimeFrame/TimeIndexStorage.hpp"

#include <armadillo>

#include <algorithm>
#include <cstdint>
#include <filesystem>
#include <iostream>

namespace {

MmapDataType stringToMmapDataType(std::string const & type_str) {
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

}// anonymous namespace

namespace {

/**
 * @brief Read a multi-channel interleaved binary file and convert to float.
 *
 * Dispatches on the binary_data_type string to call the templated
 * readBinaryFileMultiChannel with the correct element type.
 */
std::vector<std::vector<float>> readMultiChannelAsFloat(
        Loader::BinaryAnalogOptions const & loader_opts,
        std::string const & data_type_str) {

    auto convert = [](auto const & raw_channels) {
        std::vector<std::vector<float>> result;
        result.reserve(raw_channels.size());
        for (auto const & ch: raw_channels) {
            std::vector<float> fch(ch.size());
            for (std::size_t i = 0; i < ch.size(); ++i) {
                fch[i] = static_cast<float>(ch[i]);
            }
            result.push_back(std::move(fch));
        }
        return result;
    };

    if (data_type_str == "float32") {
        // float32 → float is identity but still goes through the deinterleave path
        return convert(Loader::readBinaryFileMultiChannel<float>(loader_opts));
    }
    if (data_type_str == "float64") {
        return convert(Loader::readBinaryFileMultiChannel<double>(loader_opts));
    }
    if (data_type_str == "int8") {
        return convert(Loader::readBinaryFileMultiChannel<int8_t>(loader_opts));
    }
    if (data_type_str == "uint16") {
        return convert(Loader::readBinaryFileMultiChannel<uint16_t>(loader_opts));
    }
    // Default: int16
    return convert(Loader::readBinaryFileMultiChannel<int16_t>(loader_opts));
}

/**
 * @brief Read a single-channel binary file and convert to float.
 */
std::vector<float> readSingleChannelAsFloat(
        Loader::BinaryAnalogOptions const & loader_opts,
        std::string const & data_type_str) {

    auto convert = [](auto const & raw) {
        std::vector<float> result(raw.size());
        for (std::size_t i = 0; i < raw.size(); ++i) {
            result[i] = static_cast<float>(raw[i]);
        }
        return result;
    };

    if (data_type_str == "float32") {
        return convert(Loader::readBinaryFile<float>(loader_opts));
    }
    if (data_type_str == "float64") {
        return convert(Loader::readBinaryFile<double>(loader_opts));
    }
    if (data_type_str == "int8") {
        return convert(Loader::readBinaryFile<int8_t>(loader_opts));
    }
    if (data_type_str == "uint16") {
        return convert(Loader::readBinaryFile<uint16_t>(loader_opts));
    }
    // Default: int16
    return convert(Loader::readBinaryFile<int16_t>(loader_opts));
}

}// anonymous namespace


std::vector<std::shared_ptr<AnalogTimeSeries>> load(BinaryAnalogLoaderOptions const & opts) {

    std::vector<std::shared_ptr<AnalogTimeSeries>> analog_time_series;

    // Memory-mapped loading path
    if (opts.getUseMemoryMapped()) {
        std::filesystem::path file_path = opts.filepath;
        if (!file_path.is_absolute()) {
            file_path = std::filesystem::path(opts.getParentDir()) / file_path;
        }

        // Create one memory-mapped series per channel
        for (size_t channel = 0; channel < static_cast<size_t>(opts.getNumChannels()); ++channel) {
            MmapStorageConfig config;
            config.file_path = file_path;
            config.header_size = static_cast<size_t>(opts.getHeaderSize());
            config.offset = opts.getOffset() + channel;              // Start at channel offset
            config.stride = opts.getStride() * opts.getNumChannels();// Stride accounts for all channels
            config.data_type = stringToMmapDataType(opts.getBinaryDataType());
            config.scale_factor = opts.getScaleFactor();
            config.offset_value = opts.getOffsetValue();
            config.num_samples = opts.getNumSamples();

            // Create time vector for this channel
            // First, create a temporary mmap storage to get the actual sample count
            auto temp_storage = MemoryMappedAnalogDataStorage(config);
            size_t const num_samples = temp_storage.size();

            std::vector<TimeFrameIndex> time_vector;
            for (size_t i = 0; i < num_samples; ++i) {
                time_vector.emplace_back(static_cast<int64_t>(i));
            }

            // Now create the actual series with the config
            auto series = AnalogTimeSeries::createMemoryMapped(std::move(config), std::move(time_vector));
            analog_time_series.push_back(series);
        }

        std::cout << "Memory-mapped " << analog_time_series.size() << " channel(s)" << std::endl;
        return analog_time_series;
    }

    // Traditional in-memory loading path
    auto binary_loader_opts = Loader::BinaryAnalogOptions{.file_path = opts.filepath,
                                                          .header_size_bytes = static_cast<size_t>(opts.getHeaderSize()),
                                                          .num_channels = static_cast<size_t>(opts.getNumChannels())};

    std::string const data_type_str = opts.getBinaryDataType();

    if (opts.getNumChannels() > 1) {

        auto data = readMultiChannelAsFloat(binary_loader_opts, data_type_str);

        std::cout << "Read " << data.size() << " channels" << std::endl;

        // Reserve space for all channel pointers upfront
        analog_time_series.reserve(data.size());

        for (auto & channel: data) {
            size_t const num_samples = channel.size();
            analog_time_series.push_back(std::make_shared<AnalogTimeSeries>(std::move(channel),
                                                                            num_samples));
        }

    } else {

        auto data_float = readSingleChannelAsFloat(binary_loader_opts, data_type_str);

        size_t const num_samples = data_float.size();
        analog_time_series.push_back(std::make_shared<AnalogTimeSeries>(std::move(data_float), num_samples));
    }

    return analog_time_series;
}

BinaryAnalogLoadResult loadTensorBacked(BinaryAnalogLoaderOptions const & opts) {

    // Tensor-backed applies to multi-channel loading (both in-memory and mmap)
    bool const should_use_tensor =
            opts.getUseTensorBacked() &&
            opts.getNumChannels() > 1;

    if (!should_use_tensor) {
        // Fall back to legacy per-channel loading
        auto channels = load(opts);
        return BinaryAnalogLoadResult{.tensor = nullptr, .channels = std::move(channels)};
    }

    if (opts.getUseMemoryMapped()) {
        // Block-cached mmap path: single shared mmap + decoded float block cache
        std::filesystem::path file_path = opts.filepath;
        if (!file_path.is_absolute()) {
            file_path = std::filesystem::path(opts.getParentDir()) / file_path;
        }

        auto const num_channels = static_cast<std::size_t>(opts.getNumChannels());

        SharedMmapBlockCacheConfig cache_config;
        cache_config.file_path = file_path;
        cache_config.header_size = static_cast<std::size_t>(opts.getHeaderSize());
        cache_config.num_channels = num_channels;
        cache_config.data_type = stringToMmapDataType(opts.getBinaryDataType());
        cache_config.scale_factor = opts.getScaleFactor();
        cache_config.offset_value = opts.getOffsetValue();

        auto cache = std::make_shared<SharedMmapBlockCache>(std::move(cache_config));
        auto const num_samples = cache->numSamplesPerChannel();

        // Create time storage (dense 0..N-1)
        auto time_storage = TimeIndexStorageFactory::createDenseFromZero(num_samples);

        // Build column names: "0", "1", ...
        std::vector<std::string> col_names;
        col_names.reserve(num_channels);
        for (std::size_t ch = 0; ch < num_channels; ++ch) {
            col_names.push_back(std::to_string(ch));
        }

        // Create TensorData with MmapTensorStorage
        auto mmap_storage = MmapTensorStorage(cache);
        auto tensor = std::make_shared<TensorData>(
                TensorData::createTimeSeries2DFromStorage(
                        TensorStorageWrapper{std::move(mmap_storage)},
                        time_storage,
                        nullptr,
                        std::move(col_names)));

        // Create per-channel analog views via BlockCachedMmapAnalogStorage
        std::vector<std::shared_ptr<AnalogTimeSeries>> channels;
        channels.reserve(num_channels);

        auto const_cache = std::const_pointer_cast<SharedMmapBlockCache const>(cache);
        for (std::size_t ch = 0; ch < num_channels; ++ch) {
            auto block_storage = BlockCachedMmapAnalogStorage(const_cache, ch);
            AnalogDataStorageWrapper wrapper(block_storage);

            auto analog = AnalogTimeSeries::createFromStorage(
                    std::move(wrapper), time_storage);
            channels.push_back(std::move(analog));
        }

        std::cout << "Block-cached mmap tensor loading: " << num_channels
                  << " channels, " << num_samples << " samples" << std::endl;

        return BinaryAnalogLoadResult{.tensor = std::move(tensor), .channels = std::move(channels)};
    }

    // In-memory tensor path
    auto binary_loader_opts = Loader::BinaryAnalogOptions{
            .file_path = opts.filepath,
            .header_size_bytes = static_cast<size_t>(opts.getHeaderSize()),
            .num_channels = static_cast<size_t>(opts.getNumChannels())};

    std::string const data_type_str = opts.getBinaryDataType();

    // Read interleaved binary → per-channel float vectors
    auto channel_data = readMultiChannelAsFloat(binary_loader_opts, data_type_str);

    if (channel_data.empty()) {
        return BinaryAnalogLoadResult{};
    }

    auto const num_channels = channel_data.size();
    auto const num_samples = channel_data[0].size();

    // Build an Armadillo matrix (column-major): rows = time, cols = channels
    arma::fmat matrix(num_samples, num_channels);
    for (std::size_t ch = 0; ch < num_channels; ++ch) {
        float * col = matrix.colptr(ch);
        auto const & src = channel_data[ch];
        std::copy(src.begin(), src.end(), col);
    }

    // Free the intermediate per-channel vectors
    channel_data.clear();
    channel_data.shrink_to_fit();

    // Build column names: "0", "1", ...
    std::vector<std::string> col_names;
    col_names.reserve(num_channels);
    for (std::size_t ch = 0; ch < num_channels; ++ch) {
        col_names.push_back(std::to_string(ch));
    }

    // Create time storage (dense 0..N-1)
    auto time_storage = TimeIndexStorageFactory::createDenseFromZero(num_samples);

    // Create TensorData from the Armadillo matrix (zero-copy move)
    auto tensor = std::make_shared<TensorData>(
            TensorData::createTimeSeries2D(
                    std::move(matrix),
                    time_storage,
                    nullptr,// TimeFrame will be set by DataManager on registration
                    std::move(col_names)));

    // Create zero-copy AnalogTimeSeries views via TensorColumnAnalogStorage
    std::vector<std::shared_ptr<AnalogTimeSeries>> channels;
    channels.reserve(num_channels);

    auto const_tensor = std::const_pointer_cast<TensorData const>(tensor);

    for (std::size_t ch = 0; ch < num_channels; ++ch) {
        auto col_storage = TensorColumnAnalogStorage(const_tensor, ch);
        AnalogDataStorageWrapper wrapper(col_storage);

        auto analog = AnalogTimeSeries::createFromStorage(
                std::move(wrapper), time_storage);

        channels.push_back(std::move(analog));
    }

    std::cout << "Tensor-backed loading: " << num_channels
              << " channels, " << num_samples << " samples" << std::endl;

    return BinaryAnalogLoadResult{.tensor = std::move(tensor), .channels = std::move(channels)};
}
