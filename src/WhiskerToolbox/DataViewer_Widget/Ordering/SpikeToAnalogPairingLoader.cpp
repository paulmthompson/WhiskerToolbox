/**
 * @file SpikeToAnalogPairingLoader.cpp
 * @brief Parser for CSV spike-to-analog channel pairing files.
 */

#include "Ordering/SpikeToAnalogPairingLoader.hpp"

#include <algorithm>
#include <map>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

namespace {

/// Split @p line by a single character delimiter, trimming leading/trailing
/// whitespace from each field.
[[nodiscard]] std::vector<std::string> splitByChar(std::string const & line, char delim) {
    std::vector<std::string> fields;
    std::istringstream ls(line);
    std::string field;
    while (std::getline(ls, field, delim)) {
        // Trim leading/trailing ASCII whitespace
        auto const first = field.find_first_not_of(" \t\r");
        auto const last = field.find_last_not_of(" \t\r");
        if (first == std::string::npos) {
            fields.emplace_back();
        } else {
            fields.push_back(field.substr(first, last - first + 1));
        }
    }
    return fields;
}

/// Split @p line by any whitespace (space or tab).
[[nodiscard]] std::vector<std::string> splitByWhitespace(std::string const & line) {
    std::vector<std::string> fields;
    std::istringstream ls(line);
    std::string field;
    while (ls >> field) {
        fields.push_back(field);
    }
    return fields;
}

/// Convert the delimiter enum to a char for splitByChar, or '\0' for whitespace.
[[nodiscard]] std::vector<std::string> splitLine(std::string const & line,
                                                 SpikeToAnalogCsvDelimiter delim) {
    switch (delim) {
        case SpikeToAnalogCsvDelimiter::Comma:
            return splitByChar(line, ',');
        case SpikeToAnalogCsvDelimiter::Tab:
            return splitByChar(line, '\t');
        case SpikeToAnalogCsvDelimiter::Semicolon:
            return splitByChar(line, ';');
        case SpikeToAnalogCsvDelimiter::Pipe:
            return splitByChar(line, '|');
        case SpikeToAnalogCsvDelimiter::Whitespace:
            return splitByWhitespace(line);
    }
    return splitByWhitespace(line);
}

}// namespace

std::vector<SpikeToAnalogPairing> parseSpikeToAnalogCSV(std::string const & text,
                                                        SpikeToAnalogParseConfig const & config) {
    // Map: digital_channel (0-based) -> {analog_channel (0-based) -> count}
    std::map<int, std::map<int, int>> occurrence_map;

    // Convert 1-based column indices to 0-based offsets; clamp to valid range
    int const dig_idx = config.digital_column - 1;
    int const ana_idx = config.analog_column - 1;
    int const max_idx = std::max(dig_idx, ana_idx);

    if (dig_idx < 0 || ana_idx < 0) {
        return {};
    }

    std::istringstream ss(text);
    std::string line;

    while (std::getline(ss, line)) {
        if (line.empty()) {
            continue;
        }

        auto const fields = splitLine(line, config.delimiter);

        if (static_cast<int>(fields.size()) <= max_idx) {
            continue;// not enough columns
        }

        int digital_ch = 0;
        int analog_ch = 0;
        try {
            digital_ch = std::stoi(fields[static_cast<std::size_t>(dig_idx)]);
            analog_ch = std::stoi(fields[static_cast<std::size_t>(ana_idx)]);
        } catch (std::invalid_argument const &) {
            continue;// header row or non-numeric field — skip
        } catch (std::out_of_range const &) {
            continue;
        }

        // Apply index-base conversion
        if (config.digital_one_based) {
            if (digital_ch <= 0) {
                continue;
            }
            digital_ch -= 1;
        } else {
            if (digital_ch < 0) {
                continue;
            }
        }

        if (config.analog_one_based) {
            if (analog_ch <= 0) {
                continue;
            }
            analog_ch -= 1;
        } else {
            if (analog_ch < 0) {
                continue;
            }
        }

        occurrence_map[digital_ch][analog_ch]++;
    }

    if (occurrence_map.empty()) {
        return {};
    }

    std::vector<SpikeToAnalogPairing> result;
    result.reserve(occurrence_map.size());

    for (auto const & [digital_ch, analog_counts]: occurrence_map) {
        // Find modal analog channel (highest count; tie-break: smallest index)
        int best_analog = analog_counts.begin()->first;
        int best_count = analog_counts.begin()->second;

        for (auto const & [analog_ch, count]: analog_counts) {
            if (count > best_count || (count == best_count && analog_ch < best_analog)) {
                best_analog = analog_ch;
                best_count = count;
            }
        }

        SpikeToAnalogPairing pairing;
        pairing.digital_channel = digital_ch;
        pairing.analog_channel = best_analog;
        result.push_back(pairing);
    }

    return result;
}
