#include "Line_Data_JSON.hpp"

#include "Lines/IO/CSV/Line_Data_CSV.hpp"
#include "Lines/Line_Data.hpp"
#include "loaders/loading_utils.hpp"
#include "utils/json_helpers.hpp"


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

        if (item.contains("multi_file") && item["multi_file"] == true) {
            // Multi-file CSV loading
            CSVMultiFileLineLoaderOptions opts;
            opts.parent_dir = file_path;
            
            if (item.contains("delimiter")) {
                opts.delimiter = item["delimiter"];
            }
            if (item.contains("x_column")) {
                opts.x_column = item["x_column"];
            }
            if (item.contains("y_column")) {
                opts.y_column = item["y_column"];
            }
            if (item.contains("has_header")) {
                opts.has_header = item["has_header"];
            }
            
            auto line_map = load(opts);
            line_data = std::make_shared<LineData>(line_map);
        } else {
            // Single-file CSV loading (existing functionality)
            CSVSingleFileLineLoaderOptions opts;
            opts.filepath = file_path;
            
            if (item.contains("delimiter")) {
                opts.delimiter = item["delimiter"];
            }
            if (item.contains("coordinate_delimiter")) {
                opts.coordinate_delimiter = item["coordinate_delimiter"];
            }
            if (item.contains("has_header")) {
                opts.has_header = item["has_header"];
            }
            if (item.contains("header_identifier")) {
                opts.header_identifier = item["header_identifier"];
            }
            
            auto line_map = load(opts);
            line_data = std::make_shared<LineData>(line_map);
        }

        /*

        if (!requiredFieldsExist(item,
                                 {"format"},
                                 "Error: Missing required csv fields for LineData"))
        {
            return std::make_shared<LineData>();
        }

        */

    } else if (format == "binary" || format == "capnp") {
        // Binary format is now handled by CapnProto plugin
        // This should be loaded through the plugin system
        // For now, return empty data - this should be handled by DataManager's plugin system
        std::cerr << "Warning: Binary/CapnProto format should be loaded through plugin system, not JSON loader\n";
        return std::make_shared<LineData>();

    } else if (format == "hdf5") {

    } else {

    }

    change_image_size_json(line_data, item);

    return line_data;
}
