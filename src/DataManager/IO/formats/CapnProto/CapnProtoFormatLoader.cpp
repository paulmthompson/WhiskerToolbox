#include "CapnProtoFormatLoader.hpp"

#include "linedata/Line_Data_Binary.hpp"
#include "Lines/Line_Data.hpp"

#include <iostream>

LoadResult CapnProtoFormatLoader::load(std::string const& filepath, 
                                      IODataType dataType, 
                                      nlohmann::json const& config) const {
    switch (dataType) {
        case IODataType::Line:
            return loadLineDataCapnProto(filepath, config);
            
        default:
            return LoadResult("CapnProto loader does not support data type: " + std::to_string(static_cast<int>(dataType)));
    }
}

bool CapnProtoFormatLoader::supportsFormat(std::string const& format, IODataType dataType) const {
    // Support capnp and binary formats for LineData
    if ((format == "capnp" || format == "binary") && dataType == IODataType::Line) {
        return true;
    }
    
    // Could add support for other data types in the future
    return false;
}

LoadResult CapnProtoFormatLoader::save(std::string const& filepath, 
                                       IODataType dataType, 
                                       nlohmann::json const& config, 
                                       void const* data) const {
    if (dataType != IODataType::Line) {
        return LoadResult("CapnProtoFormatLoader only supports saving LineData");
    }
    
    if (!data) {
        return LoadResult("Data pointer is null");
    }
    
    try {
        // Cast void pointer back to LineData
        auto const* line_data = static_cast<LineData const*>(data);
        
        // Convert JSON config to BinaryLineSaverOptions
        BinaryLineSaverOptions save_opts;
        save_opts.parent_dir = config.value("parent_dir", ".");
        save_opts.filename = config.value("filename", "line_data.capnp");
        
        // Call the existing save function
        if (::save(*line_data, save_opts)) {
            LoadResult result;
            result.success = true;
            return result;
        } else {
            return LoadResult("CapnProto save operation failed");
        }
        
    } catch (std::exception const& e) {
        return LoadResult("CapnProtoFormatLoader save failed: " + std::string(e.what()));
    }
}

std::string CapnProtoFormatLoader::getLoaderName() const {
    return "CapnProtoLoader";
}

LoadResult CapnProtoFormatLoader::loadLineDataCapnProto(std::string const& filepath, 
                                                       nlohmann::json const& config) const {
    try {
        // Use existing CapnProto loading functionality
        BinaryLineLoaderOptions opts;
        opts.file_path = filepath;
        
        auto loaded_line_data = ::load(opts);
        if (!loaded_line_data) {
            return LoadResult("Failed to load CapnProto LineData from: " + filepath);
        }
        
        // Extract the data map from the loaded LineData
        std::map<TimeFrameIndex, std::vector<Line2D>> line_map;
        for (auto const& time : loaded_line_data->getTimesWithData()) {
            auto line_view = loaded_line_data->getAtTime(time);
            std::vector<Line2D> line_copy(line_view.begin(), line_view.end());
            line_map[time] = line_copy;
        }
        
        // Create LineData directly
        auto line_data = std::make_shared<LineData>(line_map);
        
        // Apply image size from the loaded data
        ImageSize image_size = loaded_line_data->getImageSize();
        if (image_size.width > 0 && image_size.height > 0) {
            line_data->setImageSize(image_size);
        }
        
        // Apply image size override from config if specified
        if (config.contains("image_width") && config.contains("image_height")) {
            int width = config["image_width"];
            int height = config["image_height"];
            line_data->setImageSize(ImageSize{width, height});
        }
        
        return LoadResult(std::move(line_data));
        
    } catch (std::exception const& e) {
        return LoadResult("CapnProto loading failed: " + std::string(e.what()));
    }
}
