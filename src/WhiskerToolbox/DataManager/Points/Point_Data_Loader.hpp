#ifndef POINT_DATA_LOADER_HPP
#define POINT_DATA_LOADER_HPP

#include "Points/Point_Data.hpp"
#include "transforms/data_transforms.hpp"

#include "nlohmann/json.hpp"

#include <memory>
#include <string>

inline std::shared_ptr<PointData> load_into_PointData(std::string const & file_path, nlohmann::basic_json<> const & item) {

    int const frame_column = item["frame_column"];
    int const x_column = item["x_column"];
    int const y_column = item["y_column"];

    std::string const delim = item.value("delim", " ");

    int const height = item.value("height", -1);
    int const width = item.value("width", -1);

    int const scaled_height = item.value("scale_to_height", -1);
    int const scaled_width = item.value("scale_to_width", -1);

    auto keypoints = load_points_from_csv(
            file_path,
            frame_column,
            x_column,
            y_column,
            delim.c_str()[0]);

    std::cout << "There are " << keypoints.size() << " keypoints " << std::endl;

    auto point_data = std::make_shared<PointData>(keypoints);
    point_data->setImageSize(ImageSize{.width=width, .height=height});

    if (scaled_height > 0 && scaled_width > 0) {
        scale(point_data, ImageSize{scaled_width, scaled_height});
    }

    return point_data;
}


#endif// POINT_DATA_LOADER_HPP
