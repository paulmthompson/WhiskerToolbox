#ifndef POINT_DATA_CSV_HPP
#define POINT_DATA_CSV_HPP

#include "CoreGeometry/points.hpp"
#include "TimeFrame/TimeFrame.hpp"
#include "utils/LoaderOptionsConcepts.hpp"

#include <rfl.hpp>
#include <rfl/json.hpp>

#include <map>
#include <memory>
#include <optional>
#include <string>

class PointData;

/**
 * @struct CSVPointLoaderOptions
 *
 * @brief Options for loading point data from CSV files.
 *
 * Uses reflect-cpp for automatic JSON serialization/deserialization.
 * Optional fields can be omitted from JSON and will use default values.
 *
 * @note This struct conforms to ValidLoaderOptions concept.
 */
struct CSVPointLoaderOptions {
    std::string filepath;  // Path to the CSV file (consistent with DataManager JSON)
    
    std::optional<rfl::Validator<int, rfl::Minimum<0>>> frame_column;
    std::optional<rfl::Validator<int, rfl::Minimum<0>>> x_column;
    std::optional<rfl::Validator<int, rfl::Minimum<0>>> y_column;
    std::optional<std::string> column_delim;
    
    // Helper methods to get values with defaults
    int getFrameColumn() const { return frame_column.has_value() ? frame_column.value().value() : 0; }
    int getXColumn() const { return x_column.has_value() ? x_column.value().value() : 1; }
    int getYColumn() const { return y_column.has_value() ? y_column.value().value() : 2; }
    char getColumnDelim() const { return column_delim.has_value() && !column_delim.value().empty() ? column_delim.value()[0] : ' '; }
};

// Compile-time validation that CSVPointLoaderOptions conforms to loader requirements
static_assert(WhiskerToolbox::ValidLoaderOptions<CSVPointLoaderOptions>,
    "CSVPointLoaderOptions must have 'filepath' field and must not have 'data_type' or 'name' fields");

std::map<TimeFrameIndex, Point2D<float>> load(CSVPointLoaderOptions const & opts);

/**
 * @struct CSVPointSaverOptions
 * 
 * @brief Options for saving points to a CSV file.
 *          
 * @var CSVPointSaverOptions::filename
 * The full filepath to save the points to.
 * 
 * @var CSVPointSaverOptions::delimiter
 * The delimiter to use between columns.
 * 
 * @var CSVPointSaverOptions::line_delim
 * The line delimiter to use.
 */
struct CSVPointSaverOptions {
    std::string filename; 
    std::string parent_dir = ".";
    std::string delimiter = ",";
    std::string line_delim = "\n";
    bool save_header = true;
    std::string header = "frame,x,y";
};

void save(PointData const * point_data, CSVPointSaverOptions const & opts);

std::map<std::string, std::map<TimeFrameIndex, Point2D<float>>> load_multiple_points_from_csv(std::string const & filename, int frame_column);

/**
 * @struct DLCPointLoaderOptions
 * 
 * @brief Options for loading DLC (DeepLabCut) format CSV files.
 *
 * Uses reflect-cpp for automatic JSON serialization/deserialization.
 * Optional fields can be omitted from JSON and will use default values.
 *
 * @note This struct conforms to ValidLoaderOptions concept.
 */
struct DLCPointLoaderOptions {
    std::string filepath;  // Path to the DLC CSV file (consistent with DataManager JSON)
    
    std::optional<rfl::Validator<int, rfl::Minimum<0>>> frame_column;
    std::optional<rfl::Validator<float, rfl::Minimum<0.0f>>> likelihood_threshold;
    
    // Helper methods to get values with defaults
    int getFrameColumn() const { return frame_column.has_value() ? frame_column.value().value() : 0; }
    float getLikelihoodThreshold() const { return likelihood_threshold.has_value() ? likelihood_threshold.value().value() : 0.0f; }
};

// Compile-time validation that DLCPointLoaderOptions conforms to loader requirements
static_assert(WhiskerToolbox::ValidLoaderOptions<DLCPointLoaderOptions>,
    "DLCPointLoaderOptions must have 'filepath' field and must not have 'data_type' or 'name' fields");

std::map<std::string, std::map<TimeFrameIndex, Point2D<float>>> load_dlc_csv(DLCPointLoaderOptions const & opts);


#endif// POINT_DATA_CSV_HPP
