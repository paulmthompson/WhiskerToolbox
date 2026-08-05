#include "Point_Data_CSV.hpp"

#include "CoreUtilities/json_reflection.hpp"
#include "CoreUtilities/loading_utils.hpp"
#include "CoreUtilities/string_manip.hpp"
#include "IO/core/AtomicWrite.hpp"
#include "Points/Point_Data.hpp"

#include <cassert>
#include <charconv>
#include <cmath>
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

// Strip trailing carriage return from a string (handles Windows CRLF line endings)
inline void strip_cr(std::string & s) {
    if (!s.empty() && s.back() == '\r') {
        s.pop_back();
    }
}

std::map<TimeFrameIndex, Point2D<float>> load(CSVPointLoaderOptions const & opts) {
    std::string csv_line;

    auto line_output = std::map<TimeFrameIndex, Point2D<float>>{};

    std::fstream myfile;
    myfile.open(opts.filepath, std::fstream::in);

    std::string x_str;
    std::string y_str;
    std::string frame_str;
    std::string col_value;

    std::vector<std::pair<TimeFrameIndex, Point2D<float>>> csv_vector = {};

    // Get options with defaults via helper methods
    int const frame_column = opts.getFrameColumn();
    int const x_column = opts.getXColumn();
    int const y_column = opts.getYColumn();
    char const column_delim = opts.getColumnDelim();

    while (getline(myfile, csv_line)) {

        std::stringstream ss(csv_line);

        int cols_read = 0;
        while (getline(ss, col_value, column_delim)) {
            if (cols_read == frame_column) {
                frame_str = col_value;
            } else if (cols_read == x_column) {
                x_str = col_value;
            } else if (cols_read == y_column) {
                y_str = col_value;
            }
            cols_read++;
        }

        if (is_number(frame_str)) {
            float const x_val = std::stof(x_str);
            float const y_val = std::stof(y_str);
            if (opts.getNaNHandling() == NaNHandling::Skip && (std::isnan(x_val) || std::isnan(y_val))) {
                continue;
            }
            csv_vector.emplace_back(TimeFrameIndex(std::stoi(frame_str)), Point2D<float>{x_val, y_val});
        }
    }
    std::cout.flush();

    std::cout << "Read " << csv_vector.size() << " lines from " << opts.filepath << std::endl;

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

bool save(PointData const * point_data, CSVPointSaverOptions const & opts) {
    assert(point_data && "save: point_data must not be null");

    auto const target_path = std::filesystem::path(opts.parent_dir) / opts.filename;

    bool const ok = atomicWriteFile(target_path, [&](std::ostream & out) {
        if (opts.save_header) {
            out << opts.header << opts.line_delim;
        }

        for (auto const & [time, entity_id, point]: point_data->flattened_data()) {
            if (opts.nan_handling == NaNHandling::Skip && (std::isnan(point.x) || std::isnan(point.y))) {
                continue;
            }
            out << time.getValue();
            out << opts.delimiter << point.x << opts.delimiter << point.y;
            out << opts.line_delim;
        }
        return out.good();
    });

    if (ok) {
        std::cout << "Successfully saved points to " << target_path << std::endl;
    }
    return ok;
}

std::map<std::string, std::vector<std::pair<TimeFrameIndex, Point2D<float>>>> load_dlc_csv(DLCPointLoaderOptions const & opts) {
    std::string const & filepath = opts.filepath;

    std::ifstream file(filepath);
    if (!file.is_open()) {
        std::cerr << "Error: Could not open file " << filepath << std::endl;
        return {};
    }

    std::string ln, ele;

    // Get options with defaults via helper methods
    int const frame_column = opts.getFrameColumn();
    float const likelihood_threshold = opts.getLikelihoodThreshold();

    // Skip the "scorer" row (first row)
    getline(file, ln);

    // Read bodyparts row (second row)
    getline(file, ln);
    strip_cr(ln);// Handle Windows CRLF line endings
    std::vector<std::string> bodyparts;
    {
        std::stringstream ss(ln);
        while (getline(ss, ele, ',')) {
            bodyparts.push_back(ele);
        }
    }

    // Read coords row (third row)
    getline(file, ln);
    strip_cr(ln);// Handle Windows CRLF line endings
    std::vector<std::string> dims;
    {
        std::stringstream ss(ln);
        while (getline(ss, ele, ',')) {
            dims.push_back(ele);
        }
    }

    // Map column index to unique bodypart index
    std::vector<int> col_to_bodypart_idx(bodyparts.size(), -1);
    std::vector<std::string> unique_bodyparts;
    std::map<std::string, int> bodypart_name_to_idx;

    for (size_t i = 0; i < bodyparts.size(); ++i) {
        if (static_cast<int>(i) == frame_column) continue;
        
        std::string const & bp = bodyparts[i];
        if (bp.empty()) continue;
        
        if (bodypart_name_to_idx.find(bp) == bodypart_name_to_idx.end()) {
            bodypart_name_to_idx[bp] = static_cast<int>(unique_bodyparts.size());
            unique_bodyparts.push_back(bp);
        }
        col_to_bodypart_idx[i] = bodypart_name_to_idx[bp];
    }

    struct ParsedPoint {
        Point2D<float> point;
        float likelihood = 1.0f;
        bool has_likelihood = false;
        bool has_x = false;
        bool has_y = false;
        
        void reset() {
            point.x = 0; point.y = 0;
            likelihood = 1.0f;
            has_likelihood = false;
            has_x = false;
            has_y = false;
        }
    };
    
    std::vector<ParsedPoint> row_points(unique_bodyparts.size());
    std::map<std::string, std::vector<std::pair<TimeFrameIndex, Point2D<float>>>> data;

    // Cache pointers to the inner vectors to avoid O(log N) string lookups per row
    std::vector<std::vector<std::pair<TimeFrameIndex, Point2D<float>>>*> inner_vectors(unique_bodyparts.size());
    for (size_t i = 0; i < unique_bodyparts.size(); ++i) {
        inner_vectors[i] = &data[unique_bodyparts[i]];
    }

    while (getline(file, ln)) {
        strip_cr(ln);// Handle Windows CRLF line endings
        size_t start = 0;
        size_t col_no = 0;
        TimeFrameIndex frame_no(0);

        // Reset temporary storage for current row
        for(auto& p : row_points) {
            p.reset();
        }

        while (start <= ln.size()) {
            size_t end = ln.find(',', start);
            if (end == std::string::npos) {
                end = ln.size();
            }

            if (end > start) {
                char const * ptr = ln.c_str() + start;
                char const * ptr_end = ln.c_str() + end;
                if (static_cast<int>(col_no) == frame_column) {
                    int frame_val = 0;
                    std::from_chars(ptr, ptr_end, frame_val);
                    frame_no = TimeFrameIndex(frame_val);
                } else if (col_no < dims.size() && col_no < bodyparts.size()) {
                    int bp_idx = col_to_bodypart_idx[col_no];
                    if (bp_idx >= 0) {
                        std::string const & coord_type = dims[col_no];
                        
                        float val = 0.0f;
                        std::from_chars(ptr, ptr_end, val);

                        if (coord_type == "x") {
                            row_points[bp_idx].point.x = val;
                            row_points[bp_idx].has_x = true;
                        } else if (coord_type == "y") {
                            row_points[bp_idx].point.y = val;
                            row_points[bp_idx].has_y = true;
                        } else if (coord_type == "likelihood") {
                            row_points[bp_idx].likelihood = val;
                            row_points[bp_idx].has_likelihood = true;
                        }
                    }
                }
            }
            start = end + 1;
            ++col_no;
        }

        // Only add points that meet the likelihood threshold
        for (size_t i = 0; i < unique_bodyparts.size(); ++i) {
            auto const & rp = row_points[i];
            if (rp.has_x || rp.has_y) {
                if (!rp.has_likelihood || rp.likelihood >= likelihood_threshold) {
                    inner_vectors[i]->push_back({frame_no, rp.point});
                }
            }
        }
    }

    file.close();

    // Prune bodyparts with 0 points (created by pre-caching inner vectors)
    for (auto it = data.begin(); it != data.end(); ) {
        if (it->second.empty()) {
            it = data.erase(it);
        } else {
            ++it;
        }
    }

    return data;
}

std::map<std::string, std::shared_ptr<PointData>> load_multiple_PointData_from_dlc(std::string const & file_path, nlohmann::basic_json<> const & item) {

    // Inject filepath into JSON for reflection-based parsing
    auto json_with_path = item;
    json_with_path["filepath"] = file_path;

    // Use reflection-based parsing
    auto result = Neuralyzer::Reflection::parseJson<DLCPointLoaderOptions>(json_with_path);
    if (!result) {
        std::cerr << "Error parsing DLCPointLoaderOptions: " << result.error()->what() << std::endl;
        return {};
    }

    auto const opts = result.value();

    auto dlc_data = load_dlc_csv(opts);

    std::map<std::string, std::shared_ptr<PointData>> output;

    for (auto const & [bodypart, points]: dlc_data) {
        auto point_data = std::make_shared<PointData>(points);
        change_image_size_json(point_data, item);
        output[bodypart] = point_data;
    }

    std::cout << "Created " << output.size() << " PointData objects from DLC CSV" << std::endl;

    return output;
}
