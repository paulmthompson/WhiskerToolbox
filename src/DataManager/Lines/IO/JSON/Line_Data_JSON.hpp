#ifndef LINE_DATA_JSON_HPP
#define LINE_DATA_JSON_HPP

#include "nlohmann/json.hpp"

#include <memory>
#include <string>

class LineData;

/**
 * @brief Load LineData from JSON configuration (DEPRECATED)
 * 
 * @deprecated Use the CSVLoader/CapnProtoLoader/HDF5Loader plugins through LoaderRegistry instead.
 *             This function is kept for backward compatibility but prints warnings for most formats.
 * 
 * @param file_path Path to the line data file
 * @param item JSON configuration
 * @return Loaded LineData (may be empty for formats handled by plugins)
 */
[[deprecated("Use CSVLoader/CapnProtoLoader/HDF5Loader plugins through LoaderRegistry instead")]]
std::shared_ptr<LineData> load_into_LineData(std::string const & file_path, nlohmann::basic_json<> const & item);

#endif// LINE_DATA_JSON_HPP
