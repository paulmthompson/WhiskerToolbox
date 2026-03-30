/**
 * @file Mask_Data_CSV.hpp
 * @brief CSV loader/saver for MaskData using Run-Length Encoding (RLE)
 *
 * Provides options structs and free functions for loading and saving MaskData
 * in a CSV format where mask pixels are encoded as RLE triplets (y, x_start, length).
 *
 * CSV format:
 *   Frame,"y,x_start,length,y,x_start,length,..."
 *
 * Each row contains a frame number followed by a quoted string of RLE triplets.
 * Each triplet (y, x_start, length) represents a horizontal run of pixels at row y,
 * starting at column x_start, with the given length.
 */
#ifndef MASK_DATA_CSV_HPP
#define MASK_DATA_CSV_HPP

#include "datamanagerio_export.h"

#include "CoreGeometry/masks.hpp"
#include "ParameterSchema/ParameterSchema.hpp"
#include "TimeFrame/TimeFrame.hpp"
#include "IO/core/LoaderOptionsConcepts.hpp"

#include <rfl.hpp>
#include <rfl/json.hpp>

#include <map>
#include <optional>
#include <string>
#include <vector>

class MaskData;

/**
 * @struct CSVMaskRLELoaderOptions
 *
 * @brief Options for loading MaskData from a single CSV file using Run-Length Encoding.
 *
 * The CSV file should have a frame column followed by a quoted RLE-encoded string
 * of (y, x_start, length) triplets representing horizontal pixel runs.
 *
 * Uses reflect-cpp for automatic JSON serialization/deserialization.
 * Optional fields can be omitted from JSON and will use default values.
 *
 * @note This struct conforms to ValidLoaderOptions concept.
 */
struct CSVMaskRLELoaderOptions {
    std::string filepath;///< Path to the CSV file (required)

    std::optional<std::string> delimiter;        ///< Column delimiter (default: ",")
    std::optional<std::string> rle_delimiter;    ///< Delimiter within RLE data (default: ",")
    std::optional<bool> has_header;              ///< Whether file has a header row (default: true)
    std::optional<std::string> header_identifier;///< String to identify header row (default: "Frame")

    // Helper methods to get values with defaults
    [[nodiscard]] std::string getDelimiter() const { return delimiter.value_or(","); }
    [[nodiscard]] std::string getRLEDelimiter() const { return rle_delimiter.value_or(","); }
    [[nodiscard]] bool getHasHeader() const { return has_header.value_or(true); }
    [[nodiscard]] std::string getHeaderIdentifier() const { return header_identifier.value_or("Frame"); }
};

// Compile-time validation that CSVMaskRLELoaderOptions conforms to loader requirements
static_assert(WhiskerToolbox::ValidLoaderOptions<CSVMaskRLELoaderOptions>,
              "CSVMaskRLELoaderOptions must have 'filepath' field and must not have 'data_type' or 'name' fields");

/**
 * @struct CSVMaskRLESaverOptions
 *
 * @brief Options for saving MaskData to a single CSV file using Run-Length Encoding.
 *
 * The output CSV will have a frame column followed by a quoted RLE-encoded string
 * of (y, x_start, length) triplets.
 */
struct CSVMaskRLESaverOptions {
    std::string filename;            ///< Output filename
    std::string parent_dir = ".";    ///< Directory to save in
    std::string delimiter = ",";     ///< Column delimiter
    std::string rle_delimiter = ","; ///< Delimiter within RLE data
    bool save_header = true;         ///< Whether to write a header row
    std::string header = "Frame,RLE";///< Header text
};

/**
 * @brief Encode a Mask2D into RLE triplets string
 *
 * Converts a mask into a string of (y, x_start, length) triplets where each
 * triplet represents a contiguous horizontal run of pixels at a given y row.
 * Points are sorted by (y, x) before encoding.
 *
 * @param mask The mask to encode
 * @param rle_delimiter Delimiter between values in the RLE string
 * @return RLE-encoded string
 */
DATAMANAGERIO_EXPORT std::string encode_mask_rle(Mask2D const & mask, std::string const & rle_delimiter = ",");

/**
 * @brief Decode an RLE triplets string into a Mask2D
 *
 * Parses a string of (y, x_start, length) triplets and expands each run
 * into individual mask pixels.
 *
 * @param rle_str The RLE-encoded string
 * @param rle_delimiter Delimiter between values in the RLE string
 * @return Decoded Mask2D
 */
DATAMANAGERIO_EXPORT Mask2D decode_mask_rle(std::string const & rle_str, std::string const & rle_delimiter = ",");

/**
 * @brief Load MaskData from a single CSV file with RLE encoding
 *
 * @param opts Options controlling the load behavior
 * @return A map of timestamps to vectors of Mask2D objects
 */
DATAMANAGERIO_EXPORT std::map<TimeFrameIndex, std::vector<Mask2D>> load(CSVMaskRLELoaderOptions const & opts);

/**
 * @brief Save MaskData to a single CSV file with RLE encoding
 *
 * Uses atomic writes: data is written to a temporary file and then
 * renamed over the target to prevent corruption on crash.
 *
 * @param mask_data Non-null pointer to the MaskData to save.
 * @param opts      Saver options (directory, filename, delimiters, header).
 * @return true on success, false on I/O error.
 *
 * @pre mask_data must not be null.
 */
DATAMANAGERIO_EXPORT bool save(MaskData const * mask_data, CSVMaskRLESaverOptions const & opts);

template<>
struct ParameterUIHints<CSVMaskRLESaverOptions> {
    static void annotate(ParameterSchema & schema) {
        if (auto * f = schema.field("filename")) {
            f->tooltip = "Output filename (combined with parent_dir)";
        }
        if (auto * f = schema.field("parent_dir")) {
            f->tooltip = "Directory in which to create the output file";
        }
        if (auto * f = schema.field("delimiter")) {
            f->tooltip = "Delimiter between columns (frame and RLE data)";
        }
        if (auto * f = schema.field("rle_delimiter")) {
            f->tooltip = "Delimiter between values within RLE triplet data";
        }
        if (auto * f = schema.field("save_header")) {
            f->tooltip = "Whether to write a header row as the first line";
        }
        if (auto * f = schema.field("header")) {
            f->tooltip = "Header text to write when save_header is true";
        }
    }
};

#endif// MASK_DATA_CSV_HPP
