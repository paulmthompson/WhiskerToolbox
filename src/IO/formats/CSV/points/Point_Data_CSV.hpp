#ifndef POINT_DATA_CSV_HPP
#define POINT_DATA_CSV_HPP

#include "datamanagerio_export.h"

#include "CoreGeometry/points.hpp"
#include "IO/core/LoaderOptionsConcepts.hpp"
#include "ParameterSchema/ParameterSchema.hpp"
#include "TimeFrame/TimeFrame.hpp"

#include <nlohmann/json.hpp>
#include <rfl.hpp>
#include <rfl/json.hpp>

#include <map>
#include <memory>
#include <optional>
#include <string>

class PointData;

/**
 * @enum NaNHandling
 * @brief Controls how NaN coordinate values are treated during CSV load and save operations.
 */
enum class NaNHandling {
    Skip,  ///< Skip rows where x or y is NaN (default)
    Include///< Preserve rows with NaN coordinates
};

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
    std::string filepath;// Path to the CSV file (consistent with DataManager JSON)

    std::optional<rfl::Validator<int, rfl::Minimum<0>>> frame_column;
    std::optional<rfl::Validator<int, rfl::Minimum<0>>> x_column;
    std::optional<rfl::Validator<int, rfl::Minimum<0>>> y_column;
    std::optional<std::string> column_delim;
    std::optional<NaNHandling> nan_handling;
    std::optional<rfl::Validator<int, rfl::Minimum<1>>> height;
    std::optional<rfl::Validator<int, rfl::Minimum<1>>> width;

    // Helper methods to get values with defaults
    int getFrameColumn() const { return frame_column.has_value() ? frame_column.value().value() : 0; }
    int getXColumn() const { return x_column.has_value() ? x_column.value().value() : 1; }
    int getYColumn() const { return y_column.has_value() ? y_column.value().value() : 2; }
    char getColumnDelim() const { return column_delim.has_value() && !column_delim.value().empty() ? column_delim.value()[0] : ' '; }
    NaNHandling getNaNHandling() const { return nan_handling.value_or(NaNHandling::Skip); }
};

// Compile-time validation that CSVPointLoaderOptions conforms to loader requirements
static_assert(WhiskerToolbox::ValidLoaderOptions<CSVPointLoaderOptions>,
              "CSVPointLoaderOptions must have 'filepath' field and must not have 'data_type' or 'name' fields");

DATAMANAGERIO_EXPORT std::map<TimeFrameIndex, Point2D<float>> load(CSVPointLoaderOptions const & opts);

template<>
struct ParameterUIHints<CSVPointLoaderOptions> {
    /// @brief Annotate schema fields for AutoParamWidget (import UI).
    static void annotate(ParameterSchema & schema) {
        if (auto * f = schema.field("filepath")) {
            f->tooltip = "Path to the CSV file with frame, x, and y columns";
        }
        if (auto * f = schema.field("frame_column")) {
            f->tooltip = "0-based column index for frame number";
        }
        if (auto * f = schema.field("x_column")) {
            f->tooltip = "0-based column index for x coordinate";
        }
        if (auto * f = schema.field("y_column")) {
            f->tooltip = "0-based column index for y coordinate";
        }
        if (auto * f = schema.field("column_delim")) {
            f->tooltip = "Column separator; only the first character is used if the string is non-empty";
            f->allowed_values = {",", "\t", ";", "|", " "};
        }
        if (auto * f = schema.field("nan_handling")) {
            f->tooltip = "Whether to skip rows with NaN coordinates (Skip) or preserve them (Include)";
        }
        if (auto * f = schema.field("height")) {
            f->tooltip = "Image height in pixels (used for coordinate normalization)";
        }
        if (auto * f = schema.field("width")) {
            f->tooltip = "Image width in pixels (used for coordinate normalization)";
        }
    }
};

/**
 * @struct CSVPointSaverOptions
 * 
 * @brief Options for saving points to a CSV file.
 *
 * @var CSVPointSaverOptions::filename
 * The filename (not full path) to save the points to.
 * Combined with parent_dir to form the full output path.
 * 
 * @var CSVPointSaverOptions::parent_dir
 * The directory in which to create the output file.
 *
 * @var CSVPointSaverOptions::delimiter
 * The delimiter to use between columns.
 * 
 * @var CSVPointSaverOptions::line_delim
 * The line delimiter to use.
 *
 * @var CSVPointSaverOptions::save_header
 * Whether to write a header row as the first line.
 *
 * @var CSVPointSaverOptions::header
 * The header string to write when save_header is true.
 */
struct CSVPointSaverOptions {
    std::string filename;
    std::string parent_dir = ".";
    std::string delimiter = ",";
    std::string line_delim = "\n";
    bool save_header = true;
    std::string header = "frame,x,y";
    NaNHandling nan_handling = NaNHandling::Skip;
};

/**
 * @brief Save PointData to a CSV file.
 *
 * Uses atomic writes: data is written to a temporary file and then
 * renamed over the target to prevent corruption on crash.
 *
 * @param point_data Non-null pointer to the PointData to save.
 * @param opts       Saver options (directory, filename, delimiters, header).
 * @return true on success, false on I/O error.
 *
 * @pre point_data must not be null.
 */
DATAMANAGERIO_EXPORT bool save(PointData const * point_data, CSVPointSaverOptions const & opts);

template<>
struct ParameterUIHints<CSVPointSaverOptions> {
    /// @brief Annotate schema fields for AutoParamWidget (export UI).
    static void annotate(ParameterSchema & schema) {
        if (auto * f = schema.field("filename")) {
            f->tooltip = "Output filename (combined with parent_dir)";
        }
        if (auto * f = schema.field("parent_dir")) {
            f->tooltip = "Directory in which to create the output file";
        }
        if (auto * f = schema.field("delimiter")) {
            f->tooltip = "Delimiter between frame, x, and y columns";
            f->allowed_values = {",", "\t", ";", "|", " "};
        }
        if (auto * f = schema.field("line_delim")) {
            f->tooltip = "Line delimiter (newline character)";
        }
        if (auto * f = schema.field("save_header")) {
            f->tooltip = "Whether to write a header row as the first line";
        }
        if (auto * f = schema.field("header")) {
            f->tooltip = "Header text to write when save_header is true";
        }
        if (auto * f = schema.field("nan_handling")) {
            f->tooltip = "Whether to skip rows with NaN coordinates (Skip) or write them as 'nan' (Include)";
        }
    }
};

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
    std::string filepath;// Path to the DLC CSV file (consistent with DataManager JSON)

    std::optional<rfl::Validator<int, rfl::Minimum<0>>> frame_column;
    std::optional<rfl::Validator<float, rfl::Minimum<0.0f>>> likelihood_threshold;
    std::optional<rfl::Validator<int, rfl::Minimum<1>>> height;
    std::optional<rfl::Validator<int, rfl::Minimum<1>>> width;

    // Helper methods to get values with defaults
    int getFrameColumn() const { return frame_column.has_value() ? frame_column.value().value() : 0; }
    float getLikelihoodThreshold() const { return likelihood_threshold.has_value() ? likelihood_threshold.value().value() : 0.0f; }
};

// Compile-time validation that DLCPointLoaderOptions conforms to loader requirements
static_assert(WhiskerToolbox::ValidLoaderOptions<DLCPointLoaderOptions>,
              "DLCPointLoaderOptions must have 'filepath' field and must not have 'data_type' or 'name' fields");

DATAMANAGERIO_EXPORT std::map<std::string, std::map<TimeFrameIndex, Point2D<float>>> load_dlc_csv(DLCPointLoaderOptions const & opts);

template<>
struct ParameterUIHints<DLCPointLoaderOptions> {
    /// @brief Annotate schema fields for AutoParamWidget (import UI).
    static void annotate(ParameterSchema & schema) {
        if (auto * f = schema.field("filepath")) {
            f->tooltip = "Path to the DeepLabCut-style CSV with multi-bodypart coordinates";
        }
        if (auto * f = schema.field("frame_column")) {
            f->tooltip = "0-based column index for the frame number column";
        }
        if (auto * f = schema.field("likelihood_threshold")) {
            f->tooltip =
                    "Minimum likelihood to keep a sample when likelihood columns exist; bodyparts without likelihood are still loaded";
        }
        if (auto * f = schema.field("height")) {
            f->tooltip = "Image height in pixels (used for coordinate normalization)";
        }
        if (auto * f = schema.field("width")) {
            f->tooltip = "Image width in pixels (used for coordinate normalization)";
        }
    }
};

/**
 * @brief Load multiple PointData objects from DLC CSV format
 *
 * Used for loading DeepLabCut multi-bodypart tracking data where each bodypart
 * becomes a separate PointData object. Parses JSON configuration to extract
 * DLC loader options, loads the CSV, and applies image size configuration.
 *
 * @param file_path Path to the DLC CSV file
 * @param item JSON configuration with DLC-specific options
 * @return Map of bodypart name to PointData
 */
DATAMANAGERIO_EXPORT std::map<std::string, std::shared_ptr<PointData>> load_multiple_PointData_from_dlc(std::string const & file_path, nlohmann::basic_json<> const & item);

#endif// POINT_DATA_CSV_HPP
