#include "Image_Data_JSON.hpp"

#include "Media/IO/Image_Data_Loader.hpp"
#include "Media/Image_Data.hpp"

#include "loaders/loading_utils.hpp"

#include <iostream>

std::shared_ptr<ImageData> load_into_ImageData(std::string const & file_path, nlohmann::basic_json<> const & item) {

    auto opts = ImageLoaderOptions{};
    opts.directory_path = file_path;

    // Parse file extensions
    if (item.contains("file_extensions")) {
        opts.file_extensions.clear();
        for (auto const & ext: item["file_extensions"]) {
            opts.file_extensions.insert(ext.get<std::string>());
        }
    }

    // Parse filename pattern
    if (item.contains("filename_pattern")) {
        opts.filename_pattern = item["filename_pattern"];
    }

    // Parse sort option
    if (item.contains("sort_by_name")) {
        opts.sort_by_name = item["sort_by_name"];
    }

    // Parse display format
    if (item.contains("display_format")) {
        std::string format_str = item["display_format"];
        if (format_str == "gray" || format_str == "Gray") {
            opts.display_format = MediaData::DisplayFormat::Gray;
        } else if (format_str == "color" || format_str == "Color") {
            opts.display_format = MediaData::DisplayFormat::Color;
        } else {
            std::cout << "Warning: Unknown display format '" << format_str << "', using default Color" << std::endl;
        }
    }

    // Parse recursive search option
    if (item.contains("recursive_search")) {
        opts.recursive_search = item["recursive_search"];
    }

    auto image_data = load(opts);

    return image_data;
}