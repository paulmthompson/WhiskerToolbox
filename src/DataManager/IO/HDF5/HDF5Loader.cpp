#include "HDF5Loader.hpp"
#include "../DataFactory.hpp"
#include "../LoaderRegistry.hpp"
#include "hdf5_loaders.hpp"
#include "CoreGeometry/points.hpp"
#include "CoreGeometry/masks.hpp"
#include "CoreGeometry/lines.hpp"

#include <iostream>

std::string HDF5Loader::getFormatId() const {
    return "hdf5";
}

bool HDF5Loader::supportsDataType(IODataType data_type) const {
    using enum IODataType;
    switch (data_type) {
        case Mask:
        case Line:
            return true;
        default:
            return false;
    }
}

LoadResult HDF5Loader::loadData(
    std::string const& file_path,
    IODataType data_type,
    nlohmann::json const& config,
    DataFactory* factory
) const {
    if (!factory) {
        return LoadResult("Factory is null");
    }
    
    try {
        using enum IODataType;
        if (data_type == Mask) {
            return loadMaskData(file_path, config, factory);
        }
        if (data_type == Line) {
            return loadLineData(file_path, config, factory);
        }
        return LoadResult("Unsupported data type for HDF5 loader");
    } catch (std::exception const& e) {
        return LoadResult("HDF5 loading error: " + std::string(e.what()));
    }
}

LoadResult HDF5Loader::loadMaskData(
    std::string const& file_path,
    nlohmann::json const& config,
    DataFactory* factory
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
        
        // Convert to raw data format
        MaskDataRaw raw_data;
        
        for (std::size_t i = 0; i < frames.size(); i++) {
            int32_t frame = frames[i];
            std::vector<Mask2D> frame_masks;
            
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
                    frame_masks.push_back(std::move(mask_points));
                }
            }
            
            if (!frame_masks.empty()) {
                raw_data.time_masks[frame] = std::move(frame_masks);
            }
        }
        
        // Extract image size from config if available
        if (config.contains("image_width")) {
            raw_data.image_width = config["image_width"].get<uint32_t>();
        }
        if (config.contains("image_height")) {
            raw_data.image_height = config["image_height"].get<uint32_t>();
        }
        
        // Create MaskData using factory
        auto mask_data = factory->createMaskDataFromRaw(raw_data);
        
        std::cout << "HDF5 mask loading complete: " << frames.size() << " frames loaded" << std::endl;
        
        return LoadResult(std::move(mask_data));
        
    } catch (std::exception const& e) {
        return LoadResult("Error loading HDF5 mask data: " + std::string(e.what()));
    }
}

LoadResult HDF5Loader::loadLineData(
    std::string const& file_path,
    nlohmann::json const& config,
    DataFactory* factory
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
        
        // Convert to raw data format
        LineDataRaw raw_data;
        
        for (std::size_t i = 0; i < frames.size(); i++) {
            int32_t frame = frames[i];
            std::vector<Line2D> frame_lines;
            
            if (i < x_coords.size() && i < y_coords.size()) {
                Line2D line;
                
                auto const& x_vec = x_coords[i];
                auto const& y_vec = y_coords[i];
                
                size_t min_size = std::min(x_vec.size(), y_vec.size());
                for (size_t j = 0; j < min_size; j++) {
                    line.push_back(Point2D<float>(x_vec[j], y_vec[j]));
                }

                if (!line.empty()) {
                    frame_lines.push_back(std::move(line));
                }
            }
            
            if (!frame_lines.empty()) {
                raw_data.time_lines[frame] = std::move(frame_lines);
            }
        }
        
        // Extract image size from config if available
        if (config.contains("image_width")) {
            raw_data.image_width = config["image_width"].get<uint32_t>();
        }
        if (config.contains("image_height")) {
            raw_data.image_height = config["image_height"].get<uint32_t>();
        }
        
        // Create LineData using factory
        auto line_data = factory->createLineDataFromRaw(raw_data);
        
        std::cout << "HDF5 line loading complete: " << frames.size() << " frames loaded" << std::endl;
        
        return LoadResult(std::move(line_data));
        
    } catch (std::exception const& e) {
        return LoadResult("Error loading HDF5 line data: " + std::string(e.what()));
    }
}

// Note: HDF5 registration is now handled by the LoaderRegistration system
// The HDF5FormatLoader wraps this class for the new registry system

// Keep this function for backward compatibility - some code may still call it
extern "C" void ensure_hdf5_loader_registration() {
    // This function does nothing but ensures the object file is linked
    // Registration is now handled automatically by LoaderRegistration
}
