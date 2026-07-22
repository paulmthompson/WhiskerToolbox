/// @file JsonDataLoadExpansion.cpp
/// @brief Implementation of JSON load-config loop expansion.

#include "JsonDataLoadExpansion.hpp"

#include <spdlog/spdlog.h>

#include <cctype>
#include <map>
#include <ranges>
#include <string>
#include <vector>

namespace {

/// @brief Return true when @p c is valid in a brace placeholder identifier.
[[nodiscard]] bool isPlaceholderChar(char const c) {
    return std::isalnum(static_cast<unsigned char>(c)) != 0 || c == '_';
}

/// @brief Return true when @p name is a valid placeholder identifier.
[[nodiscard]] bool isValidPlaceholderName(std::string const & name) {
    if (name.empty()) {
        return false;
    }
    if (!std::isalpha(static_cast<unsigned char>(name.front())) && name.front() != '_') {
        return false;
    }
    return std::ranges::all_of(name, [](char const c) { return isPlaceholderChar(c); });
}

/// @brief Append placeholder names found in @p text to @p out.
void collectBracePlaceholdersInString(std::string const & text, std::set<std::string> & out) {
    for (std::size_t pos = 0; pos < text.size(); ++pos) {
        if (text[pos] != '{') {
            continue;
        }
        if (pos > 0 && text[pos - 1] == '$') {
            continue;
        }
        auto const end = text.find('}', pos + 1);
        if (end == std::string::npos) {
            continue;
        }
        auto const name = text.substr(pos + 1, end - pos - 1);
        if (isValidPlaceholderName(name)) {
            out.insert(name);
        }
    }
}

/// @brief Replace `{name}` tokens in @p text using @p bindings (skips `${...}`).
[[nodiscard]] std::string substituteBracePlaceholdersInString(
        std::string text,
        std::map<std::string, std::string> const & bindings) {
    for (std::size_t pos = 0; pos < text.size();) {
        if (text[pos] != '{') {
            ++pos;
            continue;
        }
        if (pos > 0 && text[pos - 1] == '$') {
            pos += 2;
            continue;
        }
        auto const end = text.find('}', pos + 1);
        if (end == std::string::npos) {
            ++pos;
            continue;
        }
        auto const name = text.substr(pos + 1, end - pos - 1);
        if (!isValidPlaceholderName(name)) {
            pos = end + 1;
            continue;
        }
        if (auto const it = bindings.find(name); it != bindings.end()) {
            text.replace(pos, end - pos + 1, it->second);
            pos += it->second.size();
        } else {
            pos = end + 1;
        }
    }
    return text;
}

/// @brief Substitute brace placeholders in JSON string fields using an explicit stack.
void substituteBracePlaceholdersInPlace(
        nlohmann::json & value,
        std::map<std::string, std::string> const & bindings) {
    std::vector<nlohmann::json *> stack{&value};
    while (!stack.empty()) {
        auto * node = stack.back();
        stack.pop_back();
        if (node->is_string()) {
            *node = substituteBracePlaceholdersInString(node->get<std::string>(), bindings);
            continue;
        }
        if (node->is_array()) {
            for (auto & element: *node) {
                stack.push_back(&element);
            }
            continue;
        }
        if (node->is_object()) {
            for (auto & [_, child]: node->items()) {
                stack.push_back(&child);
            }
        }
    }
}

/// @brief Convert a JSON array element to a loop iteration string.
[[nodiscard]] std::optional<std::string> jsonToLoopValueString(nlohmann::json const & element) {
    if (element.is_string()) {
        return element.get<std::string>();
    }
    if (element.is_number_integer()) {
        return std::to_string(element.get<std::int64_t>());
    }
    if (element.is_number_unsigned()) {
        return std::to_string(element.get<std::uint64_t>());
    }
    if (element.is_boolean()) {
        return element.get<bool>() ? "true" : "false";
    }
    return std::nullopt;
}

/// @brief Parse root `"loops"` into a map of iteration value lists.
[[nodiscard]] std::map<std::string, std::vector<std::string>>
parseLoopsObject(nlohmann::json const & loops) {
    std::map<std::string, std::vector<std::string>> parsed;
    if (!loops.is_object()) {
        spdlog::error("JSON load config: 'loops' must be an object");
        return parsed;
    }
    for (auto const & [name, definition]: loops.items()) {
        auto values = parseLoopDefinition(definition);
        if (!values.has_value()) {
            spdlog::error("JSON load config: invalid loop definition for '{}'", name);
            continue;
        }
        if (values->empty()) {
            spdlog::error("JSON load config: loop '{}' produced no iteration values", name);
            continue;
        }
        parsed.emplace(name, std::move(*values));
    }
    return parsed;
}

}// namespace

std::optional<std::vector<std::string>>
parseLoopDefinition(nlohmann::json const & definition) {
    if (definition.is_array()) {
        std::vector<std::string> values;
        values.reserve(definition.size());
        for (auto const & element: definition) {
            auto const as_string = jsonToLoopValueString(element);
            if (!as_string.has_value()) {
                spdlog::error("JSON load config: loop array elements must be strings or numbers");
                return std::nullopt;
            }
            values.push_back(*as_string);
        }
        return values;
    }

    if (!definition.is_object()) {
        spdlog::error("JSON load config: loop definition must be an array or range object");
        return std::nullopt;
    }

    if (!definition.contains("from") || !definition.contains("to")) {
        spdlog::error("JSON load config: loop range object requires 'from' and 'to'");
        return std::nullopt;
    }
    if (!definition["from"].is_number_integer() || !definition["to"].is_number_integer()) {
        spdlog::error("JSON load config: loop range 'from' and 'to' must be integers");
        return std::nullopt;
    }

    auto const from = definition["from"].get<std::int64_t>();
    auto const to = definition["to"].get<std::int64_t>();
    std::int64_t step = 1;
    if (definition.contains("step")) {
        if (!definition["step"].is_number_integer()) {
            spdlog::error("JSON load config: loop range 'step' must be an integer");
            return std::nullopt;
        }
        step = definition["step"].get<std::int64_t>();
    }
    if (step == 0) {
        spdlog::error("JSON load config: loop range 'step' must not be zero");
        return std::nullopt;
    }

    std::vector<std::string> values;
    if (step > 0) {
        for (std::int64_t i = from; i <= to; i += step) {
            values.push_back(std::to_string(i));
        }
    } else {
        for (std::int64_t i = from; i >= to; i += step) {
            values.push_back(std::to_string(i));
        }
    }

    if (values.empty()) {
        spdlog::error("JSON load config: loop range produced no iteration values");
        return std::nullopt;
    }
    return values;
}

std::set<std::string> collectBracePlaceholders(std::string const & text) {
    std::set<std::string> placeholders;
    collectBracePlaceholdersInString(text, placeholders);
    return placeholders;
}

std::set<std::string> collectBracePlaceholders(nlohmann::json const & value) {
    std::set<std::string> placeholders;
    std::vector<nlohmann::json const *> stack{&value};
    while (!stack.empty()) {
        auto const * node = stack.back();
        stack.pop_back();
        if (node->is_string()) {
            collectBracePlaceholdersInString(node->get<std::string>(), placeholders);
            continue;
        }
        if (node->is_array()) {
            for (auto const & element: *node) {
                stack.push_back(&element);
            }
            continue;
        }
        if (node->is_object()) {
            for (auto const & [_, child]: node->items()) {
                stack.push_back(&child);
            }
        }
    }
    return placeholders;
}

nlohmann::json substituteBracePlaceholders(
        nlohmann::json value,
        std::map<std::string, std::string> const & bindings) {
    substituteBracePlaceholdersInPlace(value, bindings);
    return value;
}

// NOLINTNEXTLINE(bugprone-easily-swappable-parameters) -- first argument is data array, second is loops map
nlohmann::json expandDataArrayLoops(
        nlohmann::json const & data,
        nlohmann::json const & loops) {
    nlohmann::json expanded = nlohmann::json::array();
    if (!data.is_array()) {
        spdlog::error("JSON load config: 'data' must be an array for loop expansion");
        return expanded;
    }

    auto const parsed_loops = parseLoopsObject(loops);

    for (auto const & item: data) {
        if (item.contains("transformations")) {
            expanded.push_back(item);
            continue;
        }

        auto const placeholders = collectBracePlaceholders(item);
        if (placeholders.empty()) {
            expanded.push_back(item);
            continue;
        }

        if (placeholders.size() != 1) {
            spdlog::error(
                    "JSON load config: data entry must use exactly one loop variable; found {}",
                    placeholders.size());
            continue;
        }

        auto const loop_name = *placeholders.begin();
        auto const loop_it = parsed_loops.find(loop_name);
        if (loop_it == parsed_loops.end()) {
            spdlog::error(
                    "JSON load config: no loop definition for placeholder '{{{}}}'",
                    loop_name);
            continue;
        }

        for (auto const & iteration_value: loop_it->second) {
            std::map<std::string, std::string> const bindings{{loop_name, iteration_value}};
            expanded.push_back(substituteBracePlaceholders(item, bindings));
        }
    }

    return expanded;
}
