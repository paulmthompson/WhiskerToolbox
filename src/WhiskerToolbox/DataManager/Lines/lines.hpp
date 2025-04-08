#ifndef DATAMANAGER_LINES_HPP
#define DATAMANAGER_LINES_HPP

#include "Points/points.hpp"

#include <chrono>
#include <cmath>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <map>
#include <sstream>
#include <vector>

using Line2D = std::vector<Point2D<float>>;


inline Line2D create_line(std::vector<float> const & x, std::vector<float> const & y) {
    auto new_line = Line2D{x.size()};

    for (std::size_t i = 0; i < x.size(); i++) {
        new_line[i] = Point2D<float>{x[i], y[i]};
    }

    return new_line;
}


inline void smooth_line(Line2D & line) {
    if (line.size() < 3) return;// No need to smooth if less than 3 points

    Line2D smoothed_line;
    smoothed_line.push_back(line.front());// First point remains the same

    for (std::size_t i = 1; i < line.size() - 1; ++i) {
        float const smoothed_x = (line[i - 1].x + line[i].x + line[i + 1].x) / 3.0f;
        float const smoothed_y = (line[i - 1].y + line[i].y + line[i + 1].y) / 3.0f;
        smoothed_line.push_back(Point2D<float>{smoothed_x, smoothed_y});
    }

    smoothed_line.push_back(line.back());// Last point remains the same
    line = std::move(smoothed_line);
}


inline std::vector<uint8_t> line_to_image(Line2D & line, int height, int width) {
    auto image = std::vector<uint8_t>(static_cast<size_t>(height * width));

    for (auto point: line) {
        auto x = std::lround(point.x);
        auto y = std::lround(point.y);

        auto index = width * y + x;
        image[index] = 255;
    }

    return image;
}


inline void save_line_as_csv(Line2D const & line, std::string const & filename, int const point_precision = 2) {
    std::fstream myfile;
    myfile.open(filename, std::fstream::out);

    myfile << std::fixed << std::setprecision(point_precision);
    for (auto & point: line) {
        myfile << point.x << "," << point.y << "\n";
    }

    myfile.close();
}

inline Line2D load_line_from_csv(std::string const & filename) {
    std::string csv_line;

    auto line_output = Line2D();

    std::fstream myfile;
    myfile.open(filename, std::fstream::in);

    std::string x_str;
    std::string y_str;

    while (getline(myfile, csv_line)) {

        std::stringstream ss(csv_line);

        getline(ss, x_str, ',');
        getline(ss, y_str);

        //std::cout << x_str << " , " << y_str << std::endl;

        line_output.push_back(Point2D<float>{std::stof(x_str), std::stof(y_str)});
    }

    return line_output;
}

inline void save_lines_csv(
        std::map<int, std::vector<Line2D>> const & data,
        std::string const & filename,
        std::string const & header = "Frame,X,Y") {

    std::ofstream file(filename);
    if (!file.is_open()) {
        throw std::runtime_error("Could not open file");
    }

    // Write the header
    file << header << "\n";

    // Write the data
    for (auto const & [frame, lines]: data) {
        for (auto const & line: lines) {
            std::ostringstream x_values;
            std::ostringstream y_values;

            for (auto const & point: line) {
                x_values << std::fixed << std::setprecision(1) << point.x << ",";
                y_values << std::fixed << std::setprecision(1) << point.y << ",";
            }

            // Remove the trailing comma
            std::string x_str = x_values.str();
            std::string y_str = y_values.str();
            if (!x_str.empty()) x_str.pop_back();
            if (!y_str.empty()) y_str.pop_back();

            file << frame << ",\"" << x_str << "\",\"" << y_str << "\"\n";
        }
    }

    file.close();
}


inline std::vector<float> parse_string_to_float_vector(std::string const & str) {
    std::vector<float> result;

    std::stringstream ss(str);
    std::string item;

    while (std::getline(ss, item, ',')) {
        result.push_back(std::stof(item));
    }
    return result;
}

inline std::map<int, std::vector<Line2D>> load_line_csv(std::string const & filepath) {

    auto t1 = std::chrono::high_resolution_clock::now();
    std::map<int, std::vector<Line2D>> data_map;
    std::ifstream file(filepath);
    if (!file.is_open()) {
        throw std::runtime_error("Could not open file: " + filepath);
    }

    std::string line;

    int loaded_lines = 0;
    while (std::getline(file, line)) {
        std::istringstream ss(line);
        std::string frame_num_str, x_str, y_str;

        std::getline(ss, frame_num_str, ',');

        if (frame_num_str == "Frame") {
            continue;
            // Skip the header line
        }

        std::getline(ss, x_str, '"');
        std::getline(ss, x_str, '"');

        std::getline(ss, y_str, '"');
        std::getline(ss, y_str, '"');

        int const frame_num = std::stoi(frame_num_str);

        std::vector<float> const x_values = parse_string_to_float_vector(x_str);
        std::vector<float> const y_values = parse_string_to_float_vector(y_str);

        if (x_values.size() != y_values.size()) {
            std::cerr << "Mismatched x and y values at frame: " << frame_num << std::endl;
            continue;
        }

        if (data_map.find(frame_num) == data_map.end()) {
            data_map[frame_num] = std::vector<Line2D>();
        }

        data_map[frame_num].emplace_back(create_line(x_values, y_values));
        loaded_lines += 1;
    }

    file.close();
    auto t2 = std::chrono::high_resolution_clock::now();

    auto duration = std::chrono::duration<double>(t2 - t1).count();
    std::cout << "Loaded " << loaded_lines << " lines from " << filepath << " in " << duration << "s" << std::endl;
    return data_map;
}
#endif// DATAMANAGER_LINES_HPP
