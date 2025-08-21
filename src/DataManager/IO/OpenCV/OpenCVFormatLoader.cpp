#include "OpenCVFormatLoader.hpp"

#include "Mask_Data_Image.hpp"
#include "Masks/Mask_Data.hpp"

#include <iostream>

LoadResult OpenCVFormatLoader::load(std::string const& filepath, 
                                   IODataType dataType, 
                                   nlohmann::json const& config, 
                                   DataFactory* factory) const {
    if (!factory) {
        return LoadResult("DataFactory is null");
    }

    switch (dataType) {
        case IODataType::Mask:
            return loadMaskDataImage(filepath, config, factory);
            
        default:
            return LoadResult("OpenCV loader does not support data type: " + std::to_string(static_cast<int>(dataType)));
    }
}

bool OpenCVFormatLoader::supportsFormat(std::string const& format, IODataType dataType) const {
    // Support image format for MaskData
    if (format == "image" && dataType == IODataType::Mask) {
        return true;
    }
    
    return false;
}

LoadResult OpenCVFormatLoader::save(std::string const& filepath, 
                                    IODataType dataType, 
                                    nlohmann::json const& config, 
                                    void const* data) const {
    if (dataType != IODataType::Mask) {
        return LoadResult("OpenCVFormatLoader only supports saving MaskData");
    }
    
    if (!data) {
        return LoadResult("Data pointer is null");
    }
    
    try {
        // Cast void pointer back to MaskData
        auto const* mask_data = static_cast<MaskData const*>(data);
        
        // Convert JSON config to ImageMaskSaverOptions
        ImageMaskSaverOptions save_opts;
        save_opts.parent_dir = config.value("parent_dir", ".");
        save_opts.image_format = config.value("image_format", "PNG");
        save_opts.filename_prefix = config.value("filename_prefix", "");
        save_opts.frame_number_padding = config.value("frame_number_padding", 4);
        save_opts.image_width = config.value("image_width", 640);
        save_opts.image_height = config.value("image_height", 480);
        save_opts.background_value = config.value("background_value", 0);
        save_opts.mask_value = config.value("mask_value", 255);
        save_opts.overwrite_existing = config.value("overwrite_existing", false);
        
        // Call the existing save function
        ::save(mask_data, save_opts);
        
        // Return success
        LoadResult result;
        result.success = true;
        return result;
        
    } catch (std::exception const& e) {
        return LoadResult("OpenCVFormatLoader save failed: " + std::string(e.what()));
    }
}

std::string OpenCVFormatLoader::getLoaderName() const {
    return "OpenCVFormatLoader";
}

LoadResult OpenCVFormatLoader::loadMaskDataImage(std::string const& filepath, 
                                                nlohmann::json const& config, 
                                                DataFactory* factory) const {
    try {
        // Configure image loading parameters
        ImageMaskLoaderOptions load_opts;
        load_opts.directory_path = filepath;  // filepath is actually directory path for images
        
        // Extract configuration with defaults
        if (config.contains("file_pattern")) {
            load_opts.file_pattern = config["file_pattern"];
        }
        if (config.contains("filename_prefix")) {
            load_opts.filename_prefix = config["filename_prefix"];
        }
        if (config.contains("frame_number_padding")) {
            load_opts.frame_number_padding = config["frame_number_padding"];
        }
        if (config.contains("threshold_value")) {
            load_opts.threshold_value = config["threshold_value"];
        }
        if (config.contains("invert_mask")) {
            load_opts.invert_mask = config["invert_mask"];
        }
        
        // Load using existing image loading functionality
        auto loaded_mask_data = ::load(load_opts);
        if (!loaded_mask_data) {
            return LoadResult("Failed to load image MaskData from: " + filepath);
        }
        
        // Convert to factory-created object
        // First extract the data from the loaded MaskData
        std::map<TimeFrameIndex, std::vector<Mask2D>> mask_map;
        for (auto const& time : loaded_mask_data->getTimesWithData()) {
            mask_map[time] = loaded_mask_data->getAtTime(time);
        }
        
        // Create new MaskData using factory
        auto mask_data_variant = factory->createMaskData();
        
        // Apply the loaded data to the factory-created object
        if (std::holds_alternative<std::shared_ptr<MaskData>>(mask_data_variant)) {
            auto new_mask_data = std::get<std::shared_ptr<MaskData>>(mask_data_variant);
            
            // Copy masks to the new object
            for (auto const& [time, masks] : mask_map) {
                for (auto const& mask : masks) {
                    new_mask_data->addAtTime(time, mask, false);
                }
            }
            
            // Apply image size from the loaded data
            ImageSize image_size = loaded_mask_data->getImageSize();
            if (image_size.width > 0 && image_size.height > 0) {
                new_mask_data->setImageSize(image_size);
            }
            
            // Apply image size override from config if specified
            if (config.contains("image_width") && config.contains("image_height")) {
                int width = config["image_width"];
                int height = config["image_height"];
                new_mask_data->setImageSize(ImageSize(width, height));
            }
            
            // Notify observers once at the end
            new_mask_data->notifyObservers();
            
            return LoadResult(std::move(mask_data_variant));
        } else {
            return LoadResult("Factory did not create MaskData object");
        }
        
    } catch (std::exception const& e) {
        return LoadResult("OpenCV image loading failed: " + std::string(e.what()));
    }
}
