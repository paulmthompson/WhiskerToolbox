#ifndef IMAGE_DATA_JSON_HPP
#define IMAGE_DATA_JSON_HPP

#include "nlohmann/json.hpp"

#include <memory>
#include <string>

class ImageData;

std::shared_ptr<ImageData> load_into_ImageData(std::string const & file_path, nlohmann::basic_json<> const & item);

#endif// IMAGE_DATA_JSON_HPP