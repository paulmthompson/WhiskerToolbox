#include "HDF5Loader.hpp"
#include "AnalogTimeSeries/Analog_Time_Series.hpp"
#include "CoreGeometry/ImageSize.hpp"
#include "CoreGeometry/lines.hpp"
#include "CoreGeometry/masks.hpp"
#include "CoreGeometry/points.hpp"
#include "DigitalTimeSeries/Digital_Event_Series.hpp"
#include "DigitalTimeSeries/Digital_Interval_Series.hpp"
#include "IO/formats/Binary/common/binary_loaders.hpp"
#include "Lines/Line_Data.hpp"
#include "Masks/Mask_Data.hpp"
#include "TimeFrame/TimeFrame.hpp"
#include "common/hdf5_utilities.hpp"
#include "hdf5_loaders.hpp"

#include <cmath>
#include <iostream>
#include <numeric>

std::string HDF5Loader::getFormatId() const {
    return "hdf5";
}

namespace {

bool parseBitPackedConfig(
        nlohmann::json const & config,
        std::string & data_key,
        int & channel,
        std::string & transition,
        int & sweep_row) {
    if (!config.contains("data_key")) {
        return false;
    }
    if (!config.contains("channel")) {
        throw std::invalid_argument("HDF5 bit-packed loader requires 'channel' in config");
    }
    if (!config.contains("transition")) {
        throw std::invalid_argument("HDF5 bit-packed loader requires 'transition' in config");
    }

    data_key = config["data_key"].get<std::string>();
    channel = config["channel"].get<int>();
    transition = config["transition"].get<std::string>();
    sweep_row = config.value("sweep_row", 0);

    if (channel < 0 || channel >= 16) {
        throw std::invalid_argument("HDF5 bit-packed loader: channel must be in range [0, 15]");
    }
    if (transition != "rising" && transition != "falling") {
        throw std::invalid_argument("HDF5 bit-packed loader: transition must be 'rising' or 'falling'");
    }
    if (sweep_row < 0) {
        throw std::invalid_argument("HDF5 bit-packed loader: sweep_row must be non-negative");
    }

    return true;
}

hdf5::HDF5FlatUnsignedOptions makeFlatUnsignedOptions(
        std::string const & file_path,
        std::string const & data_key,
        int sweep_row) {
    return hdf5::HDF5FlatUnsignedOptions{
            .filepath = file_path,
            .key = data_key,
            .sweep_row = sweep_row};
}

}// namespace

bool HDF5Loader::supportsDataType(DM_DataType data_type) const {
    using enum DM_DataType;
    switch (data_type) {
        case Mask:
        case Line:
        case DigitalEvent:
        case DigitalInterval:
        case Analog:
        case Time:
            return true;
        default:
            return false;
    }
}

LoadResult HDF5Loader::loadData(
        std::string const & file_path,
        DM_DataType data_type,
        nlohmann::json const & config) const {
    try {
        using enum DM_DataType;
        if (data_type == Mask) {
            return loadMaskData(file_path, config);
        }
        if (data_type == Line) {
            return loadLineData(file_path, config);
        }
        if (data_type == DigitalEvent) {
            return loadDigitalEventData(file_path, config);
        }
        if (data_type == DigitalInterval) {
            return loadDigitalIntervalData(file_path, config);
        }
        if (data_type == Analog) {
            return loadAnalogData(file_path, config);
        }
        if (data_type == Time) {
            return loadIdentityTimeFrameData(file_path, config);
        }
        return LoadResult("Unsupported data type for HDF5 loader");
    } catch (std::exception const & e) {
        return LoadResult("HDF5 loading error: " + std::string(e.what()));
    }
}

LoadResult HDF5Loader::loadMaskData(
        std::string const & file_path,
        nlohmann::json const & config) {
    try {
        // Extract configuration with defaults
        std::string frame_key = "frames";
        std::string x_key = "widths";
        std::string y_key = "heights";

        if (config.contains("frame_key")) {
            frame_key = config["frame_key"].get<std::string>();
        }
        if (config.contains("x_key")) {
            x_key = config["x_key"].get<std::string>();
        }
        if (config.contains("y_key")) {
            y_key = config["y_key"].get<std::string>();
        }

        // Load data using HDF5 utilities
        auto frames = Loader::read_array_hdf5({file_path, frame_key});
        auto x_coords = Loader::read_ragged_hdf5({file_path, x_key});
        auto y_coords = Loader::read_ragged_hdf5({file_path, y_key});

        if (frames.empty() && x_coords.empty() && y_coords.empty()) {
            return LoadResult("No data found in HDF5 file: " + file_path);
        }

        // Create MaskData directly
        auto mask_data = std::make_shared<MaskData>();

        for (std::size_t i = 0; i < frames.size(); i++) {
            TimeFrameIndex const frame_idx{frames[i]};

            if (i < x_coords.size() && i < y_coords.size()) {
                Mask2D mask_points;

                auto const & x_vec = x_coords[i];
                auto const & y_vec = y_coords[i];

                size_t const min_size = std::min(x_vec.size(), y_vec.size());
                for (size_t j = 0; j < min_size; j++) {
                    mask_points.push_back(Point2D<uint32_t>(
                            static_cast<uint32_t>(x_vec[j]),
                            static_cast<uint32_t>(y_vec[j])));
                }

                if (!mask_points.empty()) {
                    mask_data->addAtTime(frame_idx, std::move(mask_points), NotifyObservers::No);
                }
            }
        }

        // Extract image size from config if available
        if (config.contains("width") && config.contains("height")) {
            auto width = config["width"].get<int>();
            auto height = config["height"].get<int>();
            mask_data->setImageSize(ImageSize{width, height});
        }

        std::cout << "HDF5 mask loading complete: " << frames.size() << " frames loaded" << std::endl;

        return LoadResult(std::move(mask_data));

    } catch (std::exception const & e) {
        return LoadResult("Error loading HDF5 mask data: " + std::string(e.what()));
    }
}

LoadResult HDF5Loader::loadLineData(
        std::string const & file_path,
        nlohmann::json const & config) {
    try {
        // Extract configuration with defaults
        std::string frame_key = "frames";
        std::string x_key = "y";// Note: x and y are swapped in the original implementation
        std::string y_key = "x";

        if (config.contains("frame_key")) {
            frame_key = config["frame_key"].get<std::string>();
        }
        if (config.contains("x_key")) {
            x_key = config["x_key"].get<std::string>();
        }
        if (config.contains("y_key")) {
            y_key = config["y_key"].get<std::string>();
        }

        // Load data using HDF5 utilities
        auto frames = Loader::read_array_hdf5({file_path, frame_key});
        auto x_coords = Loader::read_ragged_hdf5({file_path, x_key});
        auto y_coords = Loader::read_ragged_hdf5({file_path, y_key});

        if (frames.empty() && x_coords.empty() && y_coords.empty()) {
            return LoadResult("No data found in HDF5 file: " + file_path);
        }

        // Create LineData directly
        auto line_data = std::make_shared<LineData>();

        for (std::size_t i = 0; i < frames.size(); i++) {
            TimeFrameIndex const frame_idx{frames[i]};

            if (i < x_coords.size() && i < y_coords.size()) {
                Line2D line;

                auto const & x_vec = x_coords[i];
                auto const & y_vec = y_coords[i];

                size_t const min_size = std::min(x_vec.size(), y_vec.size());
                for (size_t j = 0; j < min_size; j++) {
                    line.push_back(Point2D<float>(x_vec[j], y_vec[j]));
                }

                if (!line.empty()) {
                    line_data->addAtTime(frame_idx, std::move(line), NotifyObservers::No);
                }
            }
        }

        // Extract image size from config if available
        if (config.contains("image_width") && config.contains("image_height")) {
            auto width = config["image_width"].get<int>();
            auto height = config["image_height"].get<int>();
            line_data->setImageSize(ImageSize{width, height});
        }

        std::cout << "HDF5 line loading complete: " << frames.size() << " frames loaded" << std::endl;

        return LoadResult(std::move(line_data));

    } catch (std::exception const & e) {
        return LoadResult("Error loading HDF5 line data: " + std::string(e.what()));
    }
}

LoadResult HDF5Loader::loadDigitalEventData(
        std::string const & file_path,
        nlohmann::json const & config) const {
    if (config.contains("data_key")) {
        return loadBitPackedDigitalEventData(file_path, config);
    }

    try {
        // Extract required configuration
        if (!config.contains("time_key")) {
            return LoadResult("HDF5 DigitalEvent loader requires 'time_key' in config");
        }
        if (!config.contains("event_key")) {
            return LoadResult("HDF5 DigitalEvent loader requires 'event_key' in config");
        }

        std::string const time_key = config["time_key"].get<std::string>();
        std::string const event_key = config["event_key"].get<std::string>();

        // Scale factor: multiply timestamps by this to convert to frame indices
        // e.g., timestamps in seconds * 30000 Hz = sample indices
        double scale = 1.0;
        if (config.contains("scale")) {
            scale = config["scale"].get<double>();
        }

        // If true, divide by scale instead of multiply
        bool scale_divide = false;
        if (config.contains("scale_divide")) {
            scale_divide = config["scale_divide"].get<bool>();
        }

        // Load time values (float64/double)
        auto time_values = hdf5::load_array<double>({file_path, time_key});

        // Load event indicators (also as double since they're stored as float64)
        auto event_indicators = hdf5::load_array<double>({file_path, event_key});

        if (time_values.empty()) {
            return LoadResult("No time data found in HDF5 file at key: " + time_key);
        }

        if (time_values.size() != event_indicators.size()) {
            return LoadResult("HDF5 DigitalEvent: time_key and event_key arrays must have same length. "
                              "time_key has " +
                              std::to_string(time_values.size()) + " elements, "
                                                                   "event_key has " +
                              std::to_string(event_indicators.size()) + " elements.");
        }

        // Extract events where indicator is 1 (non-zero)
        std::vector<TimeFrameIndex> event_times;
        event_times.reserve(time_values.size());// Upper bound

        for (size_t i = 0; i < time_values.size(); ++i) {
            // Check if this is an event (indicator > 0.5 to handle floating point)
            if (event_indicators[i] > 0.5) {
                double scaled_time = time_values[i];

                // Apply scaling
                if (scale_divide) {
                    scaled_time /= scale;
                } else {
                    scaled_time *= scale;
                }

                // Convert to integer frame index (round to nearest)
                auto frame_idx = static_cast<int64_t>(std::round(scaled_time));
                event_times.emplace_back(frame_idx);
            }
        }

        // Create DigitalEventSeries from the event times
        auto event_series = std::make_shared<DigitalEventSeries>(std::move(event_times));

        std::cout << "HDF5 DigitalEvent loading complete: " << event_series->size()
                  << " events loaded from " << time_values.size() << " time points" << std::endl;

        return LoadResult(std::move(event_series));

    } catch (std::exception const & e) {
        return LoadResult("Error loading HDF5 DigitalEvent data: " + std::string(e.what()));
    }
}

LoadResult HDF5Loader::loadDigitalIntervalData(
        std::string const & file_path,
        nlohmann::json const & config) const {
    return loadBitPackedDigitalIntervalData(file_path, config);
}

LoadResult HDF5Loader::loadBitPackedDigitalEventData(
        std::string const & file_path,
        nlohmann::json const & config) {
    try {
        std::string data_key;
        int channel = 0;
        std::string transition;
        int sweep_row = 0;
        if (!parseBitPackedConfig(config, data_key, channel, transition, sweep_row)) {
            return LoadResult("HDF5 bit-packed DigitalEvent loader requires 'data_key' in config");
        }

        auto const samples = hdf5::load_flat_unsigned_array(
                makeFlatUnsignedOptions(file_path, data_key, sweep_row));
        if (samples.empty()) {
            return LoadResult("No bit-packed digital data found in HDF5 file at key: " + data_key);
        }

        auto const digital_data = Loader::extractDigitalData(samples, channel);
        auto const events = Loader::extractEvents(digital_data, transition);

        auto event_series = std::make_shared<DigitalEventSeries>(events);
        std::cout << "HDF5 bit-packed DigitalEvent loading complete: " << event_series->size()
                  << " events loaded from " << samples.size() << " samples" << std::endl;

        return LoadResult(std::move(event_series));
    } catch (std::exception const & e) {
        return LoadResult("Error loading HDF5 bit-packed DigitalEvent data: " + std::string(e.what()));
    }
}

LoadResult HDF5Loader::loadBitPackedDigitalIntervalData(
        std::string const & file_path,
        nlohmann::json const & config) {
    try {
        std::string data_key;
        int channel = 0;
        std::string transition;
        int sweep_row = 0;
        if (!parseBitPackedConfig(config, data_key, channel, transition, sweep_row)) {
            return LoadResult("HDF5 bit-packed DigitalInterval loader requires 'data_key' in config");
        }

        auto const samples = hdf5::load_flat_unsigned_array(
                makeFlatUnsignedOptions(file_path, data_key, sweep_row));
        if (samples.empty()) {
            return LoadResult("No bit-packed digital data found in HDF5 file at key: " + data_key);
        }

        auto const digital_data = Loader::extractDigitalData(samples, channel);
        auto const interval_pairs = Loader::extractIntervals(digital_data, transition);

        auto interval_series = std::make_shared<DigitalIntervalSeries>(interval_pairs);
        std::cout << "HDF5 bit-packed DigitalInterval loading complete: " << interval_series->size()
                  << " intervals loaded from " << samples.size() << " samples" << std::endl;

        return LoadResult(std::move(interval_series));
    } catch (std::exception const & e) {
        return LoadResult("Error loading HDF5 bit-packed DigitalInterval data: " + std::string(e.what()));
    }
}

LoadResult HDF5Loader::loadIdentityTimeFrameData(
        std::string const & file_path,
        nlohmann::json const & config) {
    try {
        if (!config.contains("data_key")) {
            return LoadResult("HDF5 identity TimeFrame loader requires 'data_key' in config");
        }

        std::string const time_layout = config.value("time_layout", "identity");
        if (time_layout != "identity") {
            return LoadResult("HDF5 Time loader only supports time_layout='identity'");
        }

        int const sweep_row = config.value("sweep_row", 0);
        if (sweep_row < 0) {
            return LoadResult("HDF5 identity TimeFrame loader: sweep_row must be non-negative");
        }

        std::string const data_key = config["data_key"].get<std::string>();
        auto const sample_count = hdf5::get_identity_sample_count(
                makeFlatUnsignedOptions(file_path, data_key, sweep_row));
        if (!sample_count.has_value() || *sample_count == 0) {
            return LoadResult("Could not determine sample count for HDF5 dataset: " + data_key);
        }

        std::vector<int> times(*sample_count);
        std::iota(times.begin(), times.end(), 0);
        auto timeframe = std::make_shared<TimeFrame>(times);

        std::cout << "HDF5 identity TimeFrame loading complete: " << times.size()
                  << " ticks from dataset " << data_key << std::endl;

        return LoadResult(std::move(timeframe));
    } catch (std::exception const & e) {
        return LoadResult("Error loading HDF5 identity TimeFrame: " + std::string(e.what()));
    }
}

LoadResult HDF5Loader::loadAnalogData(
        std::string const & file_path,
        nlohmann::json const & config) {
    try {
        // Extract required configuration
        if (!config.contains("time_key")) {
            return LoadResult("HDF5 Analog loader requires 'time_key' in config");
        }
        if (!config.contains("value_key")) {
            return LoadResult("HDF5 Analog loader requires 'value_key' in config");
        }

        std::string const time_key = config["time_key"].get<std::string>();
        std::string const value_key = config["value_key"].get<std::string>();

        // Scale factor: multiply timestamps by this to convert to frame indices
        // e.g., timestamps in seconds * 30000 Hz = sample indices
        double scale = 1.0;
        if (config.contains("scale")) {
            scale = config["scale"].get<double>();
        }

        // If true, divide by scale instead of multiply
        bool scale_divide = false;
        if (config.contains("scale_divide")) {
            scale_divide = config["scale_divide"].get<bool>();
        }

        // Load time values (float64/double)
        auto time_values = hdf5::load_array<double>({file_path, time_key});

        // Load analog values (float64/double, will be converted to float)
        auto analog_values = hdf5::load_array<double>({file_path, value_key});

        if (time_values.empty()) {
            return LoadResult("No time data found in HDF5 file at key: " + time_key);
        }

        if (analog_values.empty()) {
            return LoadResult("No analog data found in HDF5 file at key: " + value_key);
        }

        if (time_values.size() != analog_values.size()) {
            return LoadResult("HDF5 Analog: time_key and value_key arrays must have same length. "
                              "time_key has " +
                              std::to_string(time_values.size()) + " elements, "
                                                                   "value_key has " +
                              std::to_string(analog_values.size()) + " elements.");
        }

        // Build vectors for AnalogTimeSeries construction
        std::vector<TimeFrameIndex> time_indices;
        std::vector<float> values;
        time_indices.reserve(time_values.size());
        values.reserve(analog_values.size());

        for (size_t i = 0; i < time_values.size(); ++i) {
            double scaled_time = time_values[i];

            // Apply scaling
            if (scale_divide) {
                scaled_time /= scale;
            } else {
                scaled_time *= scale;
            }

            // Convert to integer frame index (round to nearest)
            auto frame_idx = static_cast<int64_t>(std::round(scaled_time));
            time_indices.emplace_back(frame_idx);

            // Convert double to float for analog value
            values.push_back(static_cast<float>(analog_values[i]));
        }

        // Create AnalogTimeSeries from time indices and values
        auto analog_series = std::make_shared<AnalogTimeSeries>(std::move(values), std::move(time_indices));

        std::cout << "HDF5 Analog loading complete: " << analog_series->getAnalogTimeSeries().size()
                  << " samples loaded" << std::endl;

        return LoadResult(std::move(analog_series));

    } catch (std::exception const & e) {
        return LoadResult("Error loading HDF5 Analog data: " + std::string(e.what()));
    }
}

// Note: HDF5 registration is now handled by the LoaderRegistration system
// The HDF5FormatLoader wraps this class for the new registry system

// Keep this function for backward compatibility - some code may still call it
extern "C" void ensure_hdf5_loader_registration() {
    // This function does nothing but ensures the object file is linked
    // Registration is now handled automatically by LoaderRegistration
}
