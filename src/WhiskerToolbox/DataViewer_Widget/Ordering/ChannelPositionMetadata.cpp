/**
 * @file ChannelPositionMetadata.cpp
 * @brief Implementation of generic probe identity parsing utilities.
 */

#include "Ordering/ChannelPositionMetadata.hpp"

#include <charconv>
#include <string>

NormalizedSeriesIdentity parseSeriesIdentity(std::string const & key) {
    NormalizedSeriesIdentity identity;
    identity.group = key;
    identity.channel_id = std::nullopt;

    auto const pos = key.rfind('_');
    if (pos == std::string::npos || pos + 1 >= key.size()) {
        return identity;
    }

    std::string const parsed_group = key.substr(0, pos);
    std::string const channel_text = key.substr(pos + 1);
    int parsed_channel = 0;
    auto const * parse_begin = channel_text.data();
    auto const * parse_end = channel_text.data() + channel_text.size();
    auto const parse_result = std::from_chars(parse_begin, parse_end, parsed_channel);
    if (parse_result.ec == std::errc{} && parse_result.ptr == parse_end) {
        identity.group = parsed_group;
        identity.channel_id = (parsed_channel > 0) ? parsed_channel - 1 : parsed_channel;
    }

    return identity;
}
