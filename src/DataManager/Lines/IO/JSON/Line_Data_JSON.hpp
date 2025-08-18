#ifndef LINE_DATA_JSON_HPP
#define LINE_DATA_JSON_HPP

#include "nlohmann/json.hpp"

#include <memory>
#include <string>

class LineData;

std::shared_ptr<LineData> load_into_LineData(std::string const & file_path, nlohmann::basic_json<> const & item);

#endif// LINE_DATA_JSON_HPP
