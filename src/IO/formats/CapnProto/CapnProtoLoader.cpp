#include "CapnProtoLoader.hpp"
#include "linedata/Line_Data_Binary.hpp"
#include "Lines/Line_Data.hpp"

#include <iostream>

CapnProtoLoader::CapnProtoLoader() {
    // Register supported data types
    _supported_types.insert(IODataType::Line);
}

std::string CapnProtoLoader::getFormatId() const {
    return "capnp";
}

bool CapnProtoLoader::supportsDataType(IODataType data_type) const {
    return _supported_types.find(data_type) != _supported_types.end();
}

LoadResult CapnProtoLoader::loadData(
    std::string const& file_path,
    IODataType data_type,
    nlohmann::json const& config
) const {
    switch (data_type) {
        case IODataType::Line:
            return loadLineData(file_path, config);
        
        default:
            return {"CapnProto loader does not support data type: " + 
                            std::to_string(static_cast<int>(data_type))};
    }
}

LoadResult CapnProtoLoader::loadLineData(std::string const& file_path, nlohmann::json const& config) const {
    try {
        // Use existing CapnProto loading functionality
        BinaryLineLoaderOptions opts;
        opts.file_path = file_path;
        
        auto loaded_line_data = ::load(opts);
        if (!loaded_line_data) {
            return LoadResult("Failed to load CapnProto LineData from: " + file_path);
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
        return {"CapnProto loading error: " + std::string(e.what())};
    }
}

// Note: CapnProto registration is now handled by the LoaderRegistration system
// The CapnProtoFormatLoader wraps this class for the new registry system
