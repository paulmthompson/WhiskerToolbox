
#include "Point_Data_JSON.hpp"

#include "Points/Point_Data.hpp"
#include "Points/IO/CSV/Point_Data_CSV.hpp"

#include "loaders/loading_utils.hpp"
#include "utils/json_reflection.hpp"

#include <iostream>

std::shared_ptr<PointData> load_into_PointData(std::string const & file_path, nlohmann::basic_json<> const & item) {

    // Inject filepath into JSON for reflection-based parsing
    auto json_with_path = item;
    json_with_path["filepath"] = file_path;

    // Check if this is a DLC CSV format
    if (item.contains("format") && item["format"] == "dlc_csv") {
        // For DLC CSV, use reflection-based parsing
        auto result = WhiskerToolbox::Reflection::parseJson<DLCPointLoaderOptions>(json_with_path);
        if (!result) {
            std::cerr << "Error parsing DLCPointLoaderOptions: " << result.error()->what() << std::endl;
            return std::make_shared<PointData>();
        }

        auto const opts = result.value();

        auto dlc_data = load_dlc_csv(opts);

        if (dlc_data.empty()) {
            std::cerr << "Warning: No data loaded from DLC CSV file" << std::endl;
            return std::make_shared<PointData>();
        }

        // Return the first bodypart data (for backward compatibility with single PointData return)
        auto first_bodypart_data = dlc_data.begin()->second;
        auto point_data = std::make_shared<PointData>(first_bodypart_data);

        change_image_size_json(point_data, item);

        return point_data;
    }

    // Original CSV loading logic - use reflection-based parsing
    auto result = WhiskerToolbox::Reflection::parseJson<CSVPointLoaderOptions>(json_with_path);
    if (!result) {
        std::cerr << "Error parsing CSVPointLoaderOptions: " << result.error()->what() << std::endl;
        return std::make_shared<PointData>();
    }

    auto opts = result.value();
    
    // Support legacy 'delim' field by mapping to 'column_delim'
    if (item.contains("delim") && !item.contains("column_delim")) {
        std::string const delim = item["delim"];
        opts.column_delim = delim;
    }

    auto keypoints = load(opts);

    std::cout << "There are " << keypoints.size() << " keypoints " << std::endl;

    auto point_data = std::make_shared<PointData>(keypoints);

    change_image_size_json(point_data, item);

    return point_data;
}

std::map<std::string, std::shared_ptr<PointData>> load_multiple_PointData_from_dlc(std::string const & file_path, nlohmann::basic_json<> const & item) {
    
    // Inject filepath into JSON for reflection-based parsing
    auto json_with_path = item;
    json_with_path["filepath"] = file_path;

    // Use reflection-based parsing
    auto result = WhiskerToolbox::Reflection::parseJson<DLCPointLoaderOptions>(json_with_path);
    if (!result) {
        std::cerr << "Error parsing DLCPointLoaderOptions: " << result.error()->what() << std::endl;
        return {};
    }

    auto const opts = result.value();

    auto dlc_data = load_dlc_csv(opts);

    std::map<std::string, std::shared_ptr<PointData>> output;
    
    for (auto const& [bodypart, points] : dlc_data) {
        auto point_data = std::make_shared<PointData>(points);
        change_image_size_json(point_data, item);
        output[bodypart] = point_data;
    }
    
    std::cout << "Created " << output.size() << " PointData objects from DLC CSV" << std::endl;
    
    return output;
}
