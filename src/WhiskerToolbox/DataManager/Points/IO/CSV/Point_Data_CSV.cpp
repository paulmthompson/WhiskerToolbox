
#include "Point_Data_CSV.hpp"

#include "Points/Point_Data.hpp"
#include "transforms/data_transforms.hpp"
#include "utils/string_manip.hpp"

#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <sstream>

std::shared_ptr<PointData> load_into_PointData(std::string const & file_path, nlohmann::basic_json<> const & item) {

    int const frame_column = item["frame_column"];
    int const x_column = item["x_column"];
    int const y_column = item["y_column"];

    std::string const delim = item.value("delim", " ");

    int const height = item.value("height", -1);
    int const width = item.value("width", -1);

    int const scaled_height = item.value("scale_to_height", -1);
    int const scaled_width = item.value("scale_to_width", -1);

    auto opts = CSVPointLoaderOptions{.filename = file_path,
                                      .frame_column = frame_column,
                                      .x_column = x_column,
                                      .y_column = y_column,
                                      .column_delim = delim.c_str()[0]};

    auto keypoints = load_points_from_csv(opts);

    std::cout << "There are " << keypoints.size() << " keypoints " << std::endl;

    auto point_data = std::make_shared<PointData>(keypoints);
    point_data->setImageSize(ImageSize{.width = width, .height = height});

    if (scaled_height > 0 && scaled_width > 0) {
        scale(point_data, ImageSize{scaled_width, scaled_height});
    }

    return point_data;
}

//https://stackoverflow.com/questions/4654636/how-to-determine-if-a-string-is-a-number-with-c
bool is_number(std::string const & s) {
    return !s.empty() && std::find_if(s.begin(),
                                      s.end(), [](unsigned char c) { return !std::isdigit(c); }) == s.end();
}

std::map<int, Point2D<float>> load_points_from_csv(CSVPointLoaderOptions const & opts) {
    std::string csv_line;

    auto line_output = std::map<int, Point2D<float>>{};

    std::fstream myfile;
    myfile.open(opts.filename, std::fstream::in);

    std::string x_str;
    std::string y_str;
    std::string frame_str;
    std::string col_value;

    std::vector<std::pair<int, Point2D<float>>> csv_vector = {};

    while (getline(myfile, csv_line)) {

        std::stringstream ss(csv_line);

        int cols_read = 0;
        while (getline(ss, col_value, opts.column_delim)) {
            if (cols_read == opts.frame_column) {
                frame_str = col_value;
            } else if (cols_read == opts.x_column) {
                x_str = col_value;
            } else if (cols_read == opts.y_column) {
                y_str = col_value;
            }
            cols_read++;
        }

        if (is_number(frame_str)) {
            //line_output[std::stoi(frame_str)]=Point2D<float>{std::stof(x_str),std::stof(y_str)};
            csv_vector.emplace_back(std::stoi(frame_str), Point2D<float>{std::stof(x_str), std::stof(y_str)});
        }
    }
    std::cout.flush();

    std::cout << "Read " << csv_vector.size() << " lines from " << opts.filename << std::endl;

    line_output.insert(csv_vector.begin(), csv_vector.end());

    return line_output;
}

std::map<std::string, std::map<int, Point2D<float>>> load_multiple_points_from_csv(std::string const & filename, int const frame_column) {
    std::fstream file;
    file.open(filename, std::fstream::in);

    std::string ln, ele;

    getline(file, ln);// skip the "scorer" row

    getline(file, ln);// bodyparts row
    std::vector<std::string> bodyparts;
    {
        std::stringstream ss(ln);
        while (getline(ss, ele, ',')) {
            bodyparts.push_back(ele);
        }
    }

    getline(file, ln);// coords row
    std::vector<std::string> dims;
    {
        std::stringstream ss(ln);
        while (getline(ss, ele, ',')) {
            dims.push_back(ele);
        }
    }

    std::map<std::string, std::map<int, Point2D<float>>> data;
    while (getline(file, ln)) {
        std::stringstream ss(ln);
        int col_no = 0;
        int frame_no = -1;
        while (getline(ss, ele, ',')) {
            if (col_no == frame_column) {
                frame_no = std::stoi(extract_numbers_from_string(ele));
            } else if (dims[col_no] == "x") {
                data[bodyparts[col_no]][frame_no].x = std::stof(ele);
            } else if (dims[col_no] == "y") {
                data[bodyparts[col_no]][frame_no].y = std::stof(ele);
            }
            ++col_no;
        }
    }

    return data;
}

void save_points_to_csv(PointData const * point_data, CSVPointSaverOptions const & opts)
{
    //Check if directory exists
    if (!std::filesystem::exists(opts.parent_dir)) {
        std::filesystem::create_directories(opts.parent_dir);
        std::cout << "Created directory: " << opts.parent_dir << std::endl;
    }

    std::string filename = opts.parent_dir + "/" + opts.filename;
    std::fstream fout;

    fout.open(filename, std::fstream::out);
    if (!fout.is_open()) {
        std::cerr << "Failed to open file for saving: " << filename << std::endl;
        return;
    }

    if (opts.save_header) {
        fout << opts.header << opts.line_delim;
    }

    for (auto const& timePointsPair : point_data->GetAllPointsAsRange()) {
        fout << timePointsPair.time;
        for (size_t i = 0; i < timePointsPair.points.size(); ++i) {
            fout << opts.delimiter << timePointsPair.points[i].x << opts.delimiter << timePointsPair.points[i].y;
        }
        fout << opts.line_delim;
    }
    fout.close();
    std::cout << "Successfully saved points to " << filename << std::endl;
}
