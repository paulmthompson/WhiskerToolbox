#include "OpenCVFormatLoader.hpp"

#include "Masks/Mask_Data.hpp"
#include "maskdata/Mask_Data_Image.hpp"

#include <iostream>

LoadResult OpenCVFormatLoader::load(std::string const & filepath,
                                    DM_DataType dataType,
                                    nlohmann::json const & config) const {
    switch (dataType) {
        case DM_DataType::Mask:
            return loadMaskDataImage(filepath, config);

        default:
            return LoadResult("OpenCV loader does not support data type: " + std::to_string(static_cast<int>(dataType)));
    }
}

bool OpenCVFormatLoader::supportsFormat(std::string const & format, DM_DataType dataType) const {
    // Support image format for MaskData
    if (format == "image" && dataType == DM_DataType::Mask) {
        return true;
    }

    return false;
}

LoadResult OpenCVFormatLoader::save(std::string const & filepath,
                                    DM_DataType dataType,
                                    nlohmann::json const & config,
                                    void const * data) const {
    if (dataType != DM_DataType::Mask) {
        return LoadResult("OpenCVFormatLoader only supports saving MaskData");
    }

    if (!data) {
        return LoadResult("Data pointer is null");
    }

    try {
        // Cast void pointer back to MaskData
        auto const * mask_data = static_cast<MaskData const *>(data);

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
        if (::save(mask_data, save_opts)) {
            LoadResult result;
            result.success = true;
            return result;
        } else {
            return LoadResult("OpenCV image mask save failed");
        }

    } catch (std::exception const & e) {
        return LoadResult("OpenCVFormatLoader save failed: " + std::string(e.what()));
    }
}

std::string OpenCVFormatLoader::getLoaderName() const {
    return "OpenCVFormatLoader";
}

std::vector<SaverInfo> OpenCVFormatLoader::getSaverInfo() const {
    using WhiskerToolbox::Transforms::V2::extractParameterSchema;
    return {
            {"image", DM_DataType::Mask, "OpenCV binary image masks (one file per frame)",
             extractParameterSchema<ImageMaskSaverOptions>()},
    };
}

LoadResult OpenCVFormatLoader::loadMaskDataImage(std::string const & filepath,
                                                 nlohmann::json const & config) {
    try {
        // Configure image loading parameters
        ImageMaskLoaderOptions load_opts;
        load_opts.directory_path = filepath;// filepath is actually directory path for images

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

        // Apply image size override from config if specified
        if (config.contains("image_width") && config.contains("image_height")) {
            int const width = config["image_width"];
            int const height = config["image_height"];
            loaded_mask_data->setImageSize(ImageSize(width, height));
        }

        return LoadResult(std::move(loaded_mask_data));

    } catch (std::exception const & e) {
        return LoadResult("OpenCV image loading failed: " + std::string(e.what()));
    }
}
