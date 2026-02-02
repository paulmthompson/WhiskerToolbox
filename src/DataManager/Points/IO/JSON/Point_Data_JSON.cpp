
#include "Point_Data_JSON.hpp"

#include "Points/Point_Data.hpp"
#include "Points/IO/CSV/Point_Data_CSV.hpp"

#include "loaders/loading_utils.hpp"
#include "utils/json_reflection.hpp"

#include <iostream>

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
