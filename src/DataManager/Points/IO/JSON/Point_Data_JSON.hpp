#ifndef POINT_DATA_JSON_HPP
#define POINT_DATA_JSON_HPP

#include "nlohmann/json.hpp"

#include <map>
#include <memory>
#include <string>

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
 *
 * @deprecated Use CSVLoader::loadPointDataDLCBatch() through LoaderRegistry instead.
 *             Cannot be removed until CSVLoader batch loading applies image size
 *             configuration (change_image_size_json) to each bodypart PointData.
 */
[[deprecated("Use CSVLoader::loadPointDataDLCBatch() via LoaderRegistry. "
             "Remove after CSVLoader batch path handles image resizing.")]]
std::map<std::string, std::shared_ptr<PointData>> load_multiple_PointData_from_dlc(std::string const & file_path, nlohmann::basic_json<> const & item);

#endif// POINT_DATA_JSON_HPP
