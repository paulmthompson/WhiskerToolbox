#include "HDF5Loader.hpp"
#include "../../core/LoaderRegistry.hpp"
#include "common/hdf5_utilities.hpp"
#include "AnalogTimeSeries/Analog_Time_Series.hpp"
#include "CoreGeometry/ImageSize.hpp"
#include "CoreGeometry/lines.hpp"
#include "CoreGeometry/masks.hpp"
#include "CoreGeometry/points.hpp"
#include "DigitalTimeSeries/Digital_Event_Series.hpp"
#include "Lines/Line_Data.hpp"
#include "Masks/Mask_Data.hpp"
#include "hdf5_loaders.hpp"

#include <cmath>
#include <iostream>

std::string HDF5Loader::getFormatId() const {
    return "hdf5";
}

bool HDF5Loader::supportsDataType(IODataType data_type) const {
    using enum IODataType;
    switch (data_type) {
        case Mask:
        case Line:
        case DigitalEvent:
        case Analog:
            return true;
        default:
            return false;
    }
}

LoadResult HDF5Loader::loadData(
    std::string const& file_path,
    IODataType data_type,
    nlohmann::json const& config
) const {
    try {
        using enum IODataType;
        if (data_type == Mask) {
            return loadMaskData(file_path, config);
        }
        if (data_type == Line) {
            return loadLineData(file_path, config);
        }
        if (data_type == DigitalEvent) {
            return loadDigitalEventData(file_path, config);
        }
        if (data_type == Analog) {
            return loadAnalogData(file_path, config);
        }
        return LoadResult("Unsupported data type for HDF5 loader");
    } catch (std::exception const& e) {
        return LoadResult("HDF5 loading error: " + std::string(e.what()));
    }
}

LoadResult HDF5Loader::loadMaskData(
    std::string const& file_path,
    nlohmann::json const& config
) const {
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
            TimeFrameIndex frame_idx{frames[i]};
            
            if (i < x_coords.size() && i < y_coords.size()) {
                Mask2D mask_points;
                
                auto const& x_vec = x_coords[i];
                auto const& y_vec = y_coords[i];
                
                size_t min_size = std::min(x_vec.size(), y_vec.size());
                for (size_t j = 0; j < min_size; j++) {
                    mask_points.push_back(Point2D<uint32_t>(
                        static_cast<uint32_t>(x_vec[j]),
                        static_cast<uint32_t>(y_vec[j]))
                    );
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
        
    } catch (std::exception const& e) {
        return LoadResult("Error loading HDF5 mask data: " + std::string(e.what()));
    }
}

LoadResult HDF5Loader::loadLineData(
    std::string const& file_path,
    nlohmann::json const& config
) const {
    try {
        // Extract configuration with defaults
        std::string frame_key = "frames";
        std::string x_key = "y";  // Note: x and y are swapped in the original implementation
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
            TimeFrameIndex frame_idx{frames[i]};
            
            if (i < x_coords.size() && i < y_coords.size()) {
                Line2D line;
                
                auto const& x_vec = x_coords[i];
                auto const& y_vec = y_coords[i];
                
                size_t min_size = std::min(x_vec.size(), y_vec.size());
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
        
    } catch (std::exception const& e) {
        return LoadResult("Error loading HDF5 line data: " + std::string(e.what()));
    }
}

LoadResult HDF5Loader::loadDigitalEventData(
    std::string const& file_path,
    nlohmann::json const& config
) const {
    try {
        // Extract required configuration
        if (!config.contains("time_key")) {
            return LoadResult("HDF5 DigitalEvent loader requires 'time_key' in config");
        }
        if (!config.contains("event_key")) {
            return LoadResult("HDF5 DigitalEvent loader requires 'event_key' in config");
        }
        
        std::string time_key = config["time_key"].get<std::string>();
        std::string event_key = config["event_key"].get<std::string>();
        
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
                            "time_key has " + std::to_string(time_values.size()) + " elements, "
                            "event_key has " + std::to_string(event_indicators.size()) + " elements.");
        }
        
        // Extract events where indicator is 1 (non-zero)
        std::vector<TimeFrameIndex> event_times;
        event_times.reserve(time_values.size());  // Upper bound
        
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
                event_times.push_back(TimeFrameIndex{frame_idx});
            }
        }
        
        // Create DigitalEventSeries from the event times
        auto event_series = std::make_shared<DigitalEventSeries>(std::move(event_times));
        
        std::cout << "HDF5 DigitalEvent loading complete: " << event_series->size() 
                  << " events loaded from " << time_values.size() << " time points" << std::endl;
        
        return LoadResult(std::move(event_series));
        
    } catch (std::exception const& e) {
        return LoadResult("Error loading HDF5 DigitalEvent data: " + std::string(e.what()));
    }
}

LoadResult HDF5Loader::loadAnalogData(
    std::string const& file_path,
    nlohmann::json const& config
) const {
    try {
        // Extract required configuration
        if (!config.contains("time_key")) {
            return LoadResult("HDF5 Analog loader requires 'time_key' in config");
        }
        if (!config.contains("value_key")) {
            return LoadResult("HDF5 Analog loader requires 'value_key' in config");
        }
        
        std::string time_key = config["time_key"].get<std::string>();
        std::string value_key = config["value_key"].get<std::string>();
        
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
                            "time_key has " + std::to_string(time_values.size()) + " elements, "
                            "value_key has " + std::to_string(analog_values.size()) + " elements.");
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
            time_indices.push_back(TimeFrameIndex{frame_idx});
            
            // Convert double to float for analog value
            values.push_back(static_cast<float>(analog_values[i]));
        }
        
        // Create AnalogTimeSeries from time indices and values
        auto analog_series = std::make_shared<AnalogTimeSeries>(std::move(values), std::move(time_indices));
        
        std::cout << "HDF5 Analog loading complete: " << analog_series->getAnalogTimeSeries().size() 
                  << " samples loaded" << std::endl;
        
        return LoadResult(std::move(analog_series));
        
    } catch (std::exception const& e) {
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
