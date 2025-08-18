
#include "Point_Data_JSON.hpp"

#include "Points/Point_Data.hpp"
#include "Points/IO/CSV/Point_Data_CSV.hpp"

#include "loaders/loading_utils.hpp"

#include <iostream>

std::shared_ptr<PointData> load_into_PointData(std::string const & file_path, nlohmann::basic_json<> const & item) {

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
