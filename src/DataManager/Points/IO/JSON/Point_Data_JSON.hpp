#ifndef POINT_DATA_JSON_HPP
#define POINT_DATA_JSON_HPP

#include "nlohmann/json.hpp"

#include <memory>
#include <string>
#include <map>

class PointData;

/**
 * @brief Load multiple PointData objects from DLC CSV format
 * 
 * Used for loading DeepLabCut multi-bodypart tracking data where each bodypart
 * becomes a separate PointData object.
 * 
 * @param file_path Path to the DLC CSV file
 * @param item JSON configuration with DLC-specific options
 * @return Map of bodypart name to PointData
 */
std::map<std::string, std::shared_ptr<PointData>> load_multiple_PointData_from_dlc(std::string const & file_path, nlohmann::basic_json<> const & item);

#endif// POINT_DATA_JSON_HPP
