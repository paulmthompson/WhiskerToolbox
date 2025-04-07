#ifndef WHISKERTOOLBOX_JSON_HELPERS_HPP
#define WHISKERTOOLBOX_JSON_HELPERS_HPP

#include "nlohmann/json.hpp"

#include <iostream>
#include <string>
#include <vector>

bool requiredFieldsExist(
        nlohmann::basic_json<> const & item,
        std::vector<std::string> const & requiredFields,
        std::string const & base_error_message) {

    std::vector<std::string> missingFields;

    for (auto const & field: requiredFields) {
        if (!item.contains(field)) {
            missingFields.push_back(field);
        }
    }

    if (!missingFields.empty()) {
        std::cout << base_error_message << std::endl;
        std::cout << "Missing required fields: ";
        for (auto const & field: missingFields) {
            std::cout << field << " ";
        }
        std::cout << std::endl;
        return false;
    }
    return true;
}

#endif//WHISKERTOOLBOX_JSON_HELPERS_HPP
