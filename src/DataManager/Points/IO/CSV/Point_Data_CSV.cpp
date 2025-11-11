#include "Point_Data_CSV.hpp"

#include "Points/Point_Data.hpp"
#include "transforms/data_transforms.hpp"
#include "utils/string_manip.hpp"

#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <sstream>

//https://stackoverflow.com/questions/4654636/how-to-determine-if-a-string-is-a-number-with-c
bool is_number(std::string const & s) {
    return !s.empty() && std::find_if(s.begin(),
                                      s.end(), [](unsigned char c) { return !std::isdigit(c); }) == s.end();
}

std::map<TimeFrameIndex, Point2D<float>> load(CSVPointLoaderOptions const & opts) {
    std::string csv_line;

    auto line_output = std::map<TimeFrameIndex, Point2D<float>>{};

    std::fstream myfile;
    myfile.open(opts.filename, std::fstream::in);

    std::string x_str;
    std::string y_str;
    std::string frame_str;
    std::string col_value;

    std::vector<std::pair<TimeFrameIndex, Point2D<float>>> csv_vector = {};

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
            //line_output[TimeFrameIndex(std::stoi(frame_str))]=Point2D<float>{std::stof(x_str),std::stof(y_str)};
            csv_vector.emplace_back(TimeFrameIndex(std::stoi(frame_str)), Point2D<float>{std::stof(x_str), std::stof(y_str)});
        }
    }
    std::cout.flush();

    std::cout << "Read " << csv_vector.size() << " lines from " << opts.filename << std::endl;

    line_output.insert(csv_vector.begin(), csv_vector.end());

    return line_output;
}

std::map<std::string, std::map<TimeFrameIndex, Point2D<float>>> load_multiple_points_from_csv(std::string const & filename, int const frame_column) {
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

    std::map<std::string, std::map<TimeFrameIndex, Point2D<float>>> data;
    while (getline(file, ln)) {
        std::stringstream ss(ln);
        size_t col_no = 0;
        TimeFrameIndex frame_no(0);
        while (getline(ss, ele, ',')) {
            if (static_cast<int>(col_no) == frame_column) {
                frame_no = TimeFrameIndex(std::stoi(extract_numbers_from_string(ele)));
            } else if (col_no < dims.size() && dims[col_no] == "x") {
                if (col_no < bodyparts.size()) {
                    data[bodyparts[col_no]][frame_no].x = std::stof(ele);
                }
            } else if (col_no < dims.size() && dims[col_no] == "y") {
                if (col_no < bodyparts.size()) {
                    data[bodyparts[col_no]][frame_no].y = std::stof(ele);
                }
            }
            ++col_no;
        }
    }

    return data;
}

void save(PointData const * point_data, CSVPointSaverOptions const & opts)
{
    //Check if directory exists
    if (!std::filesystem::exists(opts.parent_dir)) {
        std::filesystem::create_directories(opts.parent_dir);
        std::cout << "Created directory: " << opts.parent_dir << std::endl;
    }

    std::string const filename = opts.parent_dir + "/" + opts.filename;
    std::fstream fout;

    fout.open(filename, std::fstream::out);
    if (!fout.is_open()) {
        std::cerr << "Failed to open file for saving: " << filename << std::endl;
        return;
    }

    if (opts.save_header) {
        fout << opts.header << opts.line_delim;
    }

    for (auto const& [time, entries] : point_data->getAllEntries()) {
        fout << time.getValue();
        for (size_t i = 0; i < entries.size(); ++i) {
            fout << opts.delimiter << entries[i].data.x << opts.delimiter << entries[i].data.y;
        }
        fout << opts.line_delim;
    }
    fout.close();
    std::cout << "Successfully saved points to " << filename << std::endl;
}

std::map<std::string, std::map<TimeFrameIndex, Point2D<float>>> load_dlc_csv(DLCPointLoaderOptions const & opts) {
    std::fstream file;
    file.open(opts.filename, std::fstream::in);
    
    if (!file.is_open()) {
        std::cerr << "Error: Could not open file " << opts.filename << std::endl;
        return {};
    }

    std::string ln, ele;

    // Skip the "scorer" row (first row)
    getline(file, ln);

    // Read bodyparts row (second row)
    getline(file, ln);
    std::vector<std::string> bodyparts;
    {
        std::stringstream ss(ln);
        while (getline(ss, ele, ',')) {
            bodyparts.push_back(ele);
        }
    }

    // Read coords row (third row)
    getline(file, ln);
    std::vector<std::string> dims;
    {
        std::stringstream ss(ln);
        while (getline(ss, ele, ',')) {
            dims.push_back(ele);
        }
    }

    // Parse data rows
    std::map<std::string, std::map<TimeFrameIndex, Point2D<float>>> data;
    
    while (getline(file, ln)) {
        std::stringstream ss(ln);
        size_t col_no = 0;
        TimeFrameIndex frame_no(0);
        
        // Temporary storage for current row's points
        std::map<std::string, Point2D<float>> temp_points;
        std::map<std::string, float> temp_likelihoods;
        
        while (getline(ss, ele, ',')) {
            if (static_cast<int>(col_no) == opts.frame_column) {
                // For DLC CSV, frame column should already be a pure number, no extraction needed
                frame_no = TimeFrameIndex(std::stoi(ele));
            } else if (col_no < dims.size() && col_no < bodyparts.size()) {
                std::string const& bodypart = bodyparts[col_no];
                std::string const& coord_type = dims[col_no];
                
                if (coord_type == "x") {
                    temp_points[bodypart].x = std::stof(ele);
                } else if (coord_type == "y") {
                    temp_points[bodypart].y = std::stof(ele);
                } else if (coord_type == "likelihood") {
                    temp_likelihoods[bodypart] = std::stof(ele);
                }
            }
            ++col_no;
        }
        
        // Only add points that meet the likelihood threshold
        for (auto const& [bodypart, point] : temp_points) {
            auto likelihood_it = temp_likelihoods.find(bodypart);
            if (likelihood_it != temp_likelihoods.end()) {
                if (likelihood_it->second >= opts.likelihood_threshold) {
                    data[bodypart][frame_no] = point;
                }
            } else {
                // If no likelihood found, add the point (backward compatibility)
                data[bodypart][frame_no] = point;
            }
        }
    }

    file.close();
    
    std::cout << "Loaded DLC CSV with " << data.size() << " bodyparts" << std::endl;
    for (auto const& [bodypart, points] : data) {
        std::cout << "  " << bodypart << ": " << points.size() << " points" << std::endl;
    }
    
    return data;
}
