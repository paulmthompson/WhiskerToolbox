#ifndef MASK_DATA_LOADER_HPP
#define MASK_DATA_LOADER_HPP

#include "nlohmann/json.hpp"

#include <memory>
#include <string>

class MaskData;

std::shared_ptr<MaskData> load_into_MaskData(std::string const & file_path, nlohmann::basic_json<> const & item);

#endif// MASK_DATA_LOADER_HPP
