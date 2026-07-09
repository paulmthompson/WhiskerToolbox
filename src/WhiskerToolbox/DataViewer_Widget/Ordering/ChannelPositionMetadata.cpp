/**
 * @file ChannelPositionMetadata.cpp
 * @brief Implementation of generic probe identity parsing utilities.
 */

#include "Ordering/ChannelPositionMetadata.hpp"

#include <cctype>
#include <charconv>
#include <string>

namespace {

[[nodiscard]] bool hasNumericSuffix(std::string const & key, std::string & parsed_group) {
    auto const pos = key.rfind('_');
    if (pos == std::string::npos || pos + 1 >= key.size()) {
        return false;
    }

    for (size_t i = pos + 1; i < key.size(); ++i) {
        if (std::isdigit(static_cast<unsigned char>(key[i])) == 0) {
            return false;
        }
    }

    parsed_group = key.substr(0, pos);
    return true;
}

}// namespace

std::optional<int> extractChannelFromSeriesKey(
        std::string const & key,
        std::string const & group,
        bool key_one_based) {
    if (key.size() <= group.size() + 1) {
        return std::nullopt;
    }
    if (key.substr(0, group.size()) != group) {
        return std::nullopt;
    }
    if (key[group.size()] != '_') {
        return std::nullopt;
    }

    std::string const suffix = key.substr(group.size() + 1);
    int channel = 0;
    auto const * parse_begin = suffix.data();
    auto const * parse_end = suffix.data() + suffix.size();
    auto const parse_result = std::from_chars(parse_begin, parse_end, channel);
    if (parse_result.ec != std::errc{} || parse_result.ptr != parse_end) {
        return std::nullopt;
    }

    if (key_one_based) {
        if (channel <= 0) {
            return std::nullopt;
        }
        return channel - 1;
    }

    if (channel < 0) {
        return std::nullopt;
    }
    return channel;
}

bool detectKeyOneBased(
        std::vector<std::string> const & keys,
        std::string const & group) {
    for (auto const & key: keys) {
        if (key.size() <= group.size() + 1) {
            continue;
        }
        if (key.substr(0, group.size()) != group) {
            continue;
        }
        if (key[group.size()] != '_') {
            continue;
        }

        std::string const suffix = key.substr(group.size() + 1);
        int suffix_value = 0;
        auto const * parse_begin = suffix.data();
        auto const * parse_end = suffix.data() + suffix.size();
        auto const parse_result = std::from_chars(parse_begin, parse_end, suffix_value);
        if (parse_result.ec == std::errc{} && parse_result.ptr == parse_end && suffix_value == 0) {
            return false;
        }
    }

    return true;
}

NormalizedSeriesIdentity parseSeriesIdentity(std::string const & key) {
    NormalizedSeriesIdentity identity;
    identity.group = key;
    identity.channel_id = std::nullopt;

    std::string parsed_group;
    if (!hasNumericSuffix(key, parsed_group)) {
        return identity;
    }

    identity.group = parsed_group;
    if (auto const channel = extractChannelFromSeriesKey(key, parsed_group, true)) {
        identity.channel_id = channel;
    }

    return identity;
}
