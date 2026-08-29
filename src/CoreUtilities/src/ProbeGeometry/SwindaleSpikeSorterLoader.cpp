/**
 * @file SwindaleSpikeSorterLoader.cpp
 * @brief Parser and rank adapter for the Swindale SpikeSorter electrode configuration format.
 *
 * References:
 *   Swindale & Spacek (2014). Front Syst Neurosci 8:6. doi:10.3389/fnsys.2014.00006
 *   Swindale & Spacek (2015). J Comput Neurosci 38:249-267. doi:10.1007/s10827-014-0539-z
 *   Swindale et al. (2017). J Vis Exp 55217. doi:10.3791/55217
 */

#include "CoreUtilities/ProbeGeometry/SwindaleSpikeSorterLoader.hpp"

#include <algorithm>
#include <charconv>
#include <sstream>
#include <string>
#include <vector>

namespace {

[[nodiscard]] bool parseConfigFloat(std::istringstream & input, float & value) {
    std::string token;
    if (!(input >> token)) {
        return false;
    }

    while (!token.empty() && token.back() == ',') {
        token.pop_back();
    }
    if (token.empty()) {
        return false;
    }

    auto const * parse_begin = token.data();
    auto const * parse_end = token.data() + token.size();
    auto const parse_result = std::from_chars(parse_begin, parse_end, value);
    return parse_result.ec == std::errc{} && parse_result.ptr == parse_end;
}

[[nodiscard]] float lookupChannelY(std::vector<ChannelPosition> const & config,
                                   int channel_id) {
    for (auto const & position: config) {
        if (position.channel_id == channel_id) {
            return position.y;
        }
    }
    return 0.0f;
}

[[nodiscard]] int channelIdForKey(std::string const & key,
                                  std::string const & group,
                                  bool key_one_based) {
    return extractChannelFromSeriesKey(key, group, key_one_based).value_or(-1);
}

}// namespace

std::vector<ChannelPosition> parseSwindaleSpikeSorterConfig(std::string const & text) {
    std::vector<ChannelPosition> out;
    std::istringstream ss(text);
    std::string line;
    bool first = true;

    while (std::getline(ss, line)) {
        if (line.empty()) {
            continue;
        }
        if (first) {
            first = false;
            continue;// skip header row
        }

        std::istringstream ls(line);
        int row = 0;
        int ch = 0;
        float x = 0.0f;
        float y = 0.0f;

        if (!(ls >> row >> ch) || !parseConfigFloat(ls, x) || !parseConfigFloat(ls, y)) {
            continue;
        }

        // SpikeSorter uses 1-based channel numbers; convert to 0-based
        if (ch > 0) {
            ch -= 1;
        }

        ChannelPosition p;
        p.channel_id = ch;
        p.x = x;
        p.y = y;
        out.push_back(p);
    }

    return out;
}

bool extractGroupAndChannelFromKey(
        std::string const & key,
        std::string & group,
        int & channel_id) {
    auto const identity = parseSeriesIdentity(key);
    if (!identity.channel_id.has_value()) {
        group = identity.group;
        channel_id = -1;
        return false;
    }

    group = identity.group;
    channel_id = *identity.channel_id;
    return true;
}

SortableRankMap buildSwindaleSpikeSorterRanks(
        std::vector<std::string> const & keys,
        ChannelPositionMap const & configs,
        bool key_one_based) {

    struct RankedItem {
        std::string key;
        std::string group;
        int channel;
        float y_rank;
    };

    std::vector<RankedItem> ranked_items;
    ranked_items.reserve(keys.size());

    for (auto const & key: keys) {
        auto const identity = parseSeriesIdentity(key);
        int const channel = channelIdForKey(key, identity.group, key_one_based);

        float y_rank = 0.0f;
        if (auto cfg_it = configs.find(identity.group); cfg_it != configs.end()) {
            if (channel >= 0) {
                y_rank = lookupChannelY(cfg_it->second, channel);
            }
        }

        ranked_items.push_back(RankedItem{
                .key = key,
                .group = identity.group,
                .channel = channel,
                .y_rank = y_rank,
        });
    }

    std::sort(ranked_items.begin(), ranked_items.end(),
              [](RankedItem const & a, RankedItem const & b) {
                  if (a.group != b.group) {
                      return a.group < b.group;
                  }
                  if (a.y_rank != b.y_rank) {
                      return a.y_rank < b.y_rank;
                  }
                  if (a.channel != b.channel) {
                      return a.channel < b.channel;
                  }
                  return a.key < b.key;
              });

    SortableRankMap result;
    result.reserve(ranked_items.size());
    for (int rank = 0; rank < static_cast<int>(ranked_items.size()); ++rank) {
        result[ranked_items[static_cast<size_t>(rank)].key] = rank;
    }

    return result;
}

std::vector<std::string> orderKeysBySwindaleSpikeSorter(
        std::vector<std::string> const & keys,
        ChannelPositionMap const & configs,
        bool key_one_based) {

    auto const rank_map = buildSwindaleSpikeSorterRanks(keys, configs, key_one_based);

    std::vector<std::string> sorted_keys = keys;
    std::sort(sorted_keys.begin(), sorted_keys.end(),
              [&rank_map](std::string const & a, std::string const & b) {
                  int const rank_a = rank_map.count(a) > 0 ? rank_map.at(a) : 0;
                  int const rank_b = rank_map.count(b) > 0 ? rank_map.at(b) : 0;
                  return rank_a < rank_b;
              });

    return sorted_keys;
}
