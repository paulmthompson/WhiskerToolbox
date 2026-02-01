#include "Line_Data_JSON.hpp"

#include "Lines/IO/CSV/Line_Data_CSV.hpp"
#include "Lines/Line_Data.hpp"
#include "loaders/loading_utils.hpp"
#include "utils/json_helpers.hpp"
#include "utils/json_reflection.hpp"

#include <iostream>

std::shared_ptr<LineData> load_into_LineData(std::string const & file_path, nlohmann::basic_json<> const & item) {

    auto line_data = std::make_shared<LineData>();

    if (!requiredFieldsExist(item,
                             {"format"},
                             "Error: Missing required field format. Supported options include binary, csv, hdf5"))
    {
        return std::make_shared<LineData>();
    }

    auto const format = item["format"];

    if (format == "csv") {

        // Inject filepath into JSON for reflection-based parsing
        auto json_with_path = item;
        json_with_path["filepath"] = file_path;

        if (item.contains("multi_file") && item["multi_file"] == true) {
            // Multi-file CSV loading - use reflection-based parsing
            // For multi-file, parent_dir is the directory path, so inject that
            json_with_path["filepath"] = file_path;  // parent_dir for multi-file
            auto result = WhiskerToolbox::Reflection::parseJson<CSVMultiFileLineLoaderOptions>(json_with_path);
            if (!result) {
                std::cerr << "Error parsing CSVMultiFileLineLoaderOptions: " << result.error()->what() << std::endl;
                return std::make_shared<LineData>();
            }
            
            auto opts = result.value();
            // Override parent_dir with file_path since that's the directory path
            opts.parent_dir = file_path;
            
            auto line_map = load(opts);
            line_data = std::make_shared<LineData>(line_map);
        } else {
            // Single-file CSV loading - use reflection-based parsing
            auto result = WhiskerToolbox::Reflection::parseJson<CSVSingleFileLineLoaderOptions>(json_with_path);
            if (!result) {
                std::cerr << "Error parsing CSVSingleFileLineLoaderOptions: " << result.error()->what() << std::endl;
                return std::make_shared<LineData>();
            }
            
            auto const opts = result.value();
            
            auto line_map = load(opts);
            line_data = std::make_shared<LineData>(line_map);
        }

    } else if (format == "binary" || format == "capnp") {
        // Binary format is now handled by CapnProto plugin
        // This should be loaded through the plugin system
        std::cerr << "Warning: Binary/CapnProto format should be loaded through plugin system, not JSON loader\n";
        return std::make_shared<LineData>();

    } else if (format == "hdf5") {
        // HDF5 format is now handled by HDF5 plugin
        // This should be loaded through the plugin system
        std::cerr << "Warning: HDF5 format should be loaded through plugin system, not JSON loader\n";
        return std::make_shared<LineData>();

    } else {
        std::cerr << "Warning: Unknown format '" << format << "' - should be handled through plugin system\n";
        return std::make_shared<LineData>();
    }

    change_image_size_json(line_data, item);

    return line_data;
}
