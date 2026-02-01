#ifndef MASK_DATA_JSON_HPP
#define MASK_DATA_JSON_HPP

#include "nlohmann/json.hpp"

#include <memory>
#include <string>

class MaskData;

/**
 * @brief Load MaskData from JSON configuration (DEPRECATED)
 * 
 * @deprecated Use the HDF5Loader/OpenCVLoader plugins through LoaderRegistry instead.
 *             This function only prints warnings and returns empty MaskData.
 * 
 * @param file_path Path to the mask data file
 * @param item JSON configuration
 * @return Empty MaskData (all formats handled by plugins)
 */
[[deprecated("Use HDF5Loader/OpenCVLoader plugins through LoaderRegistry instead")]]
std::shared_ptr<MaskData> load_into_MaskData(std::string const & file_path, nlohmann::basic_json<> const & item);

#endif// MASK_DATA_JSON_HPP
