/**
 * @file VariableSubstitution.cpp
 * @brief Implementation of ${variable} substitution for command sequences
 */

#include "VariableSubstitution.hpp"

#include <algorithm>
#include <cctype>

namespace commands {

namespace {

/// @brief Check if a string represents a valid JSON number (integer or float).
bool isJsonNumber(std::string const & s) {
    if (s.empty()) {
        return false;
    }
    // Try to match: optional minus, digits, optional decimal part, optional exponent
    auto it = s.begin();
    if (*it == '-') {
        ++it;
    }
    if (it == s.end() || !std::isdigit(static_cast<unsigned char>(*it))) {
        return false;
    }
    while (it != s.end() && std::isdigit(static_cast<unsigned char>(*it))) {
        ++it;
    }
    if (it != s.end() && *it == '.') {
        ++it;
        if (it == s.end() || !std::isdigit(static_cast<unsigned char>(*it))) {
            return false;
        }
        while (it != s.end() && std::isdigit(static_cast<unsigned char>(*it))) {
            ++it;
        }
    }
    if (it != s.end() && (*it == 'e' || *it == 'E')) {
        ++it;
        if (it != s.end() && (*it == '+' || *it == '-')) {
            ++it;
        }
        if (it == s.end() || !std::isdigit(static_cast<unsigned char>(*it))) {
            return false;
        }
        while (it != s.end() && std::isdigit(static_cast<unsigned char>(*it))) {
            ++it;
        }
    }
    return it == s.end();
}

/// @brief Check if a string is a JSON boolean or null literal.
bool isJsonLiteral(std::string const & s) {
    return s == "true" || s == "false" || s == "null";
}

}// namespace

std::string substituteVariables(
        std::string const & input,
        std::map<std::string, std::string> const & variables) {

    std::string result = input;
    std::string::size_type pos = 0;

    while ((pos = result.find("${", pos)) != std::string::npos) {
        auto const end_pos = result.find('}', pos + 2);
        if (end_pos == std::string::npos) {
            // Malformed variable reference — skip past "${"
            pos += 2;
            continue;
        }

        auto const var_name = result.substr(pos + 2, end_pos - pos - 2);
        auto const it = variables.find(var_name);

        if (it != variables.end()) {
            auto const & value = it->second;

            // Check if ${var} is the entire content of a JSON string value: "..."
            // If preceding char is '"' and following char is '"', and the replacement
            // is a JSON number/boolean/null, strip the quotes to preserve JSON typing.
            bool const has_leading_quote = (pos > 0 && result[pos - 1] == '"');
            bool const has_trailing_quote = (end_pos + 1 < result.size() && result[end_pos + 1] == '"');

            if (has_leading_quote && has_trailing_quote &&
                (isJsonNumber(value) || isJsonLiteral(value))) {
                // Replace the entire "..." including the surrounding quotes
                result.replace(pos - 1, end_pos - pos + 3, value);
                pos = pos - 1 + value.length();
            } else {
                result.replace(pos, end_pos - pos + 1, value);
                pos += value.length();
            }
        } else {
            // Variable not found — leave as-is and skip past it
            pos = end_pos + 1;
        }
    }

    return result;
}

std::string normalizeJsonNumbers(std::string const & json) {
    std::string result;
    result.reserve(json.size());

    size_t i = 0;
    while (i < json.size()) {
        // Skip strings verbatim (including escape sequences)
        if (json[i] == '"') {
            result += '"';
            ++i;
            while (i < json.size() && json[i] != '"') {
                if (json[i] == '\\' && i + 1 < json.size()) {
                    result += json[i];
                    result += json[i + 1];
                    i += 2;
                } else {
                    result += json[i];
                    ++i;
                }
            }
            if (i < json.size()) {
                result += '"';
                ++i;
            }
            continue;
        }

        // Detect numeric tokens outside of strings
        if (json[i] == '-' || (json[i] >= '0' && json[i] <= '9')) {
            auto const start = i;
            if (json[i] == '-') {
                ++i;
            }
            while (i < json.size() && json[i] >= '0' && json[i] <= '9') {
                ++i;
            }

            auto const decimal_start = i;
            bool has_decimal = false;
            if (i < json.size() && json[i] == '.') {
                has_decimal = true;
                ++i;
                while (i < json.size() && json[i] >= '0' && json[i] <= '9') {
                    ++i;
                }
            }

            bool has_exponent = false;
            if (i < json.size() && (json[i] == 'e' || json[i] == 'E')) {
                has_exponent = true;
                ++i;
                if (i < json.size() && (json[i] == '+' || json[i] == '-')) {
                    ++i;
                }
                while (i < json.size() && json[i] >= '0' && json[i] <= '9') {
                    ++i;
                }
            }

            // Normalize: if decimal part is all zeros and no exponent, emit integer
            if (has_decimal && !has_exponent) {
                bool all_zeros = true;
                for (size_t j = decimal_start + 1; j < i; ++j) {
                    if (json[j] != '0') {
                        all_zeros = false;
                        break;
                    }
                }
                if (all_zeros && decimal_start > start) {
                    result.append(json, start, decimal_start - start);
                    continue;
                }
            }

            result.append(json, start, i - start);
            continue;
        }

        result += json[i];
        ++i;
    }

    return result;
}

}// namespace commands
