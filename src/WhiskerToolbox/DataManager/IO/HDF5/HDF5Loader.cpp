#include "HDF5Loader.hpp"
#include "../DataFactory.hpp"
#include "../LoaderRegistry.hpp"
#include "hdf5_loaders.hpp"
#include <iostream>

std::string HDF5Loader::getFormatId() const {
    return "hdf5";
}

bool HDF5Loader::supportsDataType(IODataType data_type) const {
    using enum IODataType;
    switch (data_type) {
        case Mask:
            return true;
        // case Line:  // Future support
        //     return true;
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
            std::vector<std::vector<std::pair<uint32_t, uint32_t>>> frame_masks;
            
            if (i < x_coords.size() && i < y_coords.size()) {
                std::vector<std::pair<uint32_t, uint32_t>> mask_points;
                
                auto const& x_vec = x_coords[i];
                auto const& y_vec = y_coords[i];
                
                size_t min_size = std::min(x_vec.size(), y_vec.size());
                for (size_t j = 0; j < min_size; j++) {
                    mask_points.emplace_back(
                        static_cast<uint32_t>(x_vec[j]),
                        static_cast<uint32_t>(y_vec[j])
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

// Register the HDF5 loader
REGISTER_LOADER(HDF5Loader);

// Force linking of this object file - call this function to ensure registration
extern "C" void ensure_hdf5_loader_registration() {
    // This function does nothing but ensures the object file is linked
}
