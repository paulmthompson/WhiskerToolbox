
#include "Point_Data_JSON.hpp"

#include "Points/Point_Data.hpp"
#include "Points/IO/CSV/Point_Data_CSV.hpp"

#include "loaders/loading_utils.hpp"

#include <iostream>

std::shared_ptr<PointData> load_into_PointData(std::string const & file_path, nlohmann::basic_json<> const & item) {

    // Check if this is a DLC CSV format
    if (item.contains("format") && item["format"] == "dlc_csv") {
        // For DLC CSV, we need to return the first bodypart as the main PointData
        // The caller (DataManager) will need to handle multiple bodyparts differently
        
        int const frame_column = item.value("frame_column", 0);
        float const likelihood_threshold = item.value("likelihood_threshold", 0.0f);

        auto opts = DLCPointLoaderOptions{
            .filename = file_path,
            .frame_column = frame_column,
            .likelihood_threshold = likelihood_threshold
        };

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

    // Original CSV loading logic
    int const frame_column = item["frame_column"];
    int const x_column = item["x_column"];
    int const y_column = item["y_column"];

    std::string const delim = item.value("delim", " ");

    auto opts = CSVPointLoaderOptions{.filename = file_path,
                                      .frame_column = frame_column,
                                      .x_column = x_column,
                                      .y_column = y_column,
                                      .column_delim = delim.c_str()[0]};

    auto keypoints = load(opts);

    std::cout << "There are " << keypoints.size() << " keypoints " << std::endl;

    auto point_data = std::make_shared<PointData>(keypoints);

    change_image_size_json(point_data, item);

    return point_data;
}

std::map<std::string, std::shared_ptr<PointData>> load_multiple_PointData_from_dlc(std::string const & file_path, nlohmann::basic_json<> const & item) {
    
    int const frame_column = item.value("frame_column", 0);
    float const likelihood_threshold = item.value("likelihood_threshold", 0.0f);

    auto opts = DLCPointLoaderOptions{
        .filename = file_path,
        .frame_column = frame_column,
        .likelihood_threshold = likelihood_threshold
    };

    auto dlc_data = load_dlc_csv(opts);

    std::map<std::string, std::shared_ptr<PointData>> result;
    
    for (auto const& [bodypart, points] : dlc_data) {
        auto point_data = std::make_shared<PointData>(points);
        change_image_size_json(point_data, item);
        result[bodypart] = point_data;
    }
    
    std::cout << "Created " << result.size() << " PointData objects from DLC CSV" << std::endl;
    
    return result;
}
