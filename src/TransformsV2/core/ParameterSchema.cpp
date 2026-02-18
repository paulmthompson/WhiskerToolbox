#include "ParameterSchema.hpp"

#include <algorithm>
#include <cctype>
#include <charconv>
#include <sstream>
#include <string_view>

namespace WhiskerToolbox::Transforms::V2 {

// ============================================================================
// snakeCaseToDisplay — "scale_factor" → "Scale Factor"
// ============================================================================

std::string snakeCaseToDisplay(std::string const & snake) {
    if (snake.empty()) {
        return {};
    }

    std::string result;
    result.reserve(snake.size() + 4);// slight over-allocation for spaces

    bool next_upper = true;
    for (char ch : snake) {
        if (ch == '_') {
            result += ' ';
            next_upper = true;
        } else if (next_upper) {
            result += static_cast<char>(std::toupper(static_cast<unsigned char>(ch)));
            next_upper = false;
        } else {
            result += ch;
        }
    }

    return result;
}

// ============================================================================
// Type String Parsing Helpers
// ============================================================================

namespace {

/// Remove leading/trailing whitespace from a string_view
std::string_view trim(std::string_view sv) {
    while (!sv.empty() && std::isspace(static_cast<unsigned char>(sv.front()))) {
        sv.remove_prefix(1);
    }
    while (!sv.empty() && std::isspace(static_cast<unsigned char>(sv.back()))) {
        sv.remove_suffix(1);
    }
    return sv;
}

/// Check if string_view contains a substring (case-sensitive)
bool contains(std::string_view haystack, std::string_view needle) {
    return haystack.find(needle) != std::string_view::npos;
}

/// Attempt to extract a numeric value from a constraint template parameter
/// Handles formats like:
///   "rfl::ExclusiveMinimum<0>" or "rfl::Minimum<1.5>"
///   Also handles scientific notation from Clang: "0.000000e+00f" etc.
std::optional<double> extractNumericFromConstraint(std::string_view constraint_str) {
    // Look for the template argument between < and >
    auto open = constraint_str.find('<');
    if (open == std::string_view::npos) {
        return std::nullopt;
    }

    // Find matching closing >
    auto close = constraint_str.rfind('>');
    if (close == std::string_view::npos || close <= open) {
        return std::nullopt;
    }

    auto arg_str = trim(constraint_str.substr(open + 1, close - open - 1));
    if (arg_str.empty()) {
        return std::nullopt;
    }

    // Remove any trailing type suffix (f, F, l, L, etc.)
    std::string cleaned(arg_str);
    while (!cleaned.empty() && (cleaned.back() == 'f' || cleaned.back() == 'F' ||
                                cleaned.back() == 'l' || cleaned.back() == 'L')) {
        cleaned.pop_back();
    }

    // Remove parentheses if present (Clang sometimes wraps in parens)
    if (cleaned.size() >= 2 && cleaned.front() == '(' && cleaned.back() == ')') {
        cleaned = cleaned.substr(1, cleaned.size() - 2);
    }

    // Remove trailing type suffix again (after removing parens)
    while (!cleaned.empty() && (cleaned.back() == 'f' || cleaned.back() == 'F' ||
                                cleaned.back() == 'l' || cleaned.back() == 'L')) {
        cleaned.pop_back();
    }

    if (cleaned.empty()) {
        return std::nullopt;
    }

    // Try to parse as double
    try {
        size_t pos = 0;
        double val = std::stod(cleaned, &pos);
        if (pos > 0) {
            return val;
        }
    } catch (...) {
        // Parsing failed
    }

    return std::nullopt;
}

/// Find a constraint substring and extract its numeric value
/// Looks for pattern like "ExclusiveMinimum<...>" within the type string
std::optional<double> findConstraintValue(
        std::string_view type_str,
        std::string_view constraint_name) {
    auto pos = type_str.find(constraint_name);
    if (pos == std::string_view::npos) {
        return std::nullopt;
    }

    // Extract from the constraint name to the closing >
    auto remaining = type_str.substr(pos);
    auto open = remaining.find('<');
    if (open == std::string_view::npos) {
        return std::nullopt;
    }

    // Count angle brackets to find matching close
    int depth = 0;
    size_t close_pos = open;
    for (size_t i = open; i < remaining.size(); ++i) {
        if (remaining[i] == '<') {
            ++depth;
        }
        if (remaining[i] == '>') {
            --depth;
            if (depth == 0) {
                close_pos = i;
                break;
            }
        }
    }

    auto constraint_substr = remaining.substr(0, close_pos + 1);
    return extractNumericFromConstraint(std::string(constraint_substr));
}

/// Determine the base type name by looking at the innermost type
/// Strips optional<>, Validator<>, etc. and returns the core type
std::string determineBaseType(std::string_view type_str) {
    // Check for common base types (search from innermost outward)
    // Order matters: check "double" before checking for "float" in case
    // the string contains "double"

    if (contains(type_str, "bool")) {
        return "bool";
    }
    if (contains(type_str, "double")) {
        return "double";
    }
    if (contains(type_str, "float")) {
        return "float";
    }
    if (contains(type_str, "int")) {
        return "int";
    }
    if (contains(type_str, "string")) {
        return "std::string";
    }

    // Unknown type — return the raw string
    return std::string(type_str);
}

}// anonymous namespace

// ============================================================================
// parseUnderlyingType
// ============================================================================

std::string parseUnderlyingType(std::string const & type_str) {
    return determineBaseType(type_str);
}

// ============================================================================
// isOptionalType
// ============================================================================

bool isOptionalType(std::string const & type_str) {
    return contains(type_str, "optional");
}

// ============================================================================
// hasValidator
// ============================================================================

bool hasValidator(std::string const & type_str) {
    return contains(type_str, "Validator");
}

// ============================================================================
// extractConstraints
// ============================================================================

ConstraintInfo extractConstraints(std::string const & type_str) {
    ConstraintInfo info;

    // Check for ExclusiveMinimum first (before Minimum, since Minimum is a substring)
    if (contains(type_str, "ExclusiveMinimum")) {
        info.is_exclusive_min = true;
        info.min_value = findConstraintValue(type_str, "ExclusiveMinimum");
    } else if (contains(type_str, "Minimum")) {
        info.is_exclusive_min = false;
        info.min_value = findConstraintValue(type_str, "Minimum");
    }

    // Check for ExclusiveMaximum first (before Maximum)
    if (contains(type_str, "ExclusiveMaximum")) {
        info.is_exclusive_max = true;
        info.max_value = findConstraintValue(type_str, "ExclusiveMaximum");
    } else if (contains(type_str, "Maximum")) {
        info.is_exclusive_max = false;
        info.max_value = findConstraintValue(type_str, "Maximum");
    }

    return info;
}

}// namespace WhiskerToolbox::Transforms::V2
