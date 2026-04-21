/**
 * @file SwindaleSpikeSorterLoader.cpp
 * @brief Parser and rank adapter for the Swindale SpikeSorter electrode configuration format.
 *
 * Reference:
 *   Swindale NV, Spacek MA. "SpikeSorter: Sorting large sets of waveforms from
 *   tetrode recordings." eNeuro. 2014. PMID: 28287541.
 */

#include "Ordering/SwindaleSpikeSorterLoader.hpp"

#include <algorithm>
#include <limits>
#include <sstream>
#include <string>
#include <vector>

namespace {

[[nodiscard]] float lookupChannelY(std::vector<ChannelPosition> const & config,
                                   int channel_id) {
    for (auto const & position: config) {
        if (position.channel_id == channel_id) {
            return position.y;
        }
    }
    return 0.0f;
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

        if (!(ls >> row >> ch >> x >> y)) {
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
        ChannelPositionMap const & configs) {

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
        int const channel = identity.channel_id.value_or(-1);

        float y_rank = 0.0f;
        if (auto cfg_it = configs.find(identity.group); cfg_it != configs.end()) {
            y_rank = lookupChannelY(cfg_it->second, channel);
        }

        ranked_items.push_back(RankedItem{key, identity.group, channel, y_rank});
    }

    std::stable_sort(ranked_items.begin(), ranked_items.end(),
                     [&configs](RankedItem const & a, RankedItem const & b) {
                         if (a.group != b.group) {
                             return a.group < b.group;
                         }

                         bool const group_has_config = configs.contains(a.group);
                         if (group_has_config) {
                             if (a.y_rank != b.y_rank) {
                                 return a.y_rank < b.y_rank;
                             }
                         }

                         if (a.channel != b.channel) {
                             return a.channel < b.channel;
                         }

                         return a.key < b.key;
                     });

    SortableRankMap ranks;
    for (int index = 0; index < static_cast<int>(ranked_items.size()); ++index) {
        ranks[ranked_items[static_cast<size_t>(index)].key] = index;
    }

    return ranks;
}

std::vector<std::string> orderKeysBySwindaleSpikeSorter(
        std::vector<std::string> const & keys,
        ChannelPositionMap const & configs) {

    auto const rank_map = buildSwindaleSpikeSorterRanks(keys, configs);

    struct Item {
        std::string key;
        int rank;
        int insertion_index;
    };

    std::vector<Item> items;
    items.reserve(keys.size());

    int insertion_index = 0;
    for (auto const & key: keys) {
        int const rank = rank_map.contains(key) ? rank_map.at(key) : std::numeric_limits<int>::max();
        items.push_back(Item{key, rank, insertion_index++});
    }

    std::stable_sort(items.begin(), items.end(), [](Item const & a, Item const & b) {
        if (a.rank != b.rank) {
            return a.rank < b.rank;
        }
        if (a.key != b.key) {
            return a.key < b.key;
        }
        return a.insertion_index < b.insertion_index;
    });

    std::vector<std::string> result;
    result.reserve(items.size());
    for (auto const & it: items) {
        result.push_back(it.key);
    }

    return result;
}
