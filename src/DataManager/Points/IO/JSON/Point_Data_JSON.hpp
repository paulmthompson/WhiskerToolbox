#ifndef POINT_DATA_JSON_HPP
#define POINT_DATA_JSON_HPP

#include "nlohmann/json.hpp"

#include <memory>
#include <string>

class PointData;

std::shared_ptr<PointData> load_into_PointData(std::string const & file_path, nlohmann::basic_json<> const & item);

#endif// POINT_DATA_JSON_HPP
