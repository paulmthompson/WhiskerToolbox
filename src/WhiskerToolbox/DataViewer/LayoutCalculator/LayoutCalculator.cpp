#include "LayoutCalculator.hpp"

#include <algorithm>
#include <cmath>
#include <iostream>
#include <sstream>
#include <iomanip>


void LayoutCalculator::loadAnalogSpikeSorterConfiguration(std::string const & group_name,
                                                        std::vector<AnalogGroupChannelPosition> const & positions) {
    _analog_group_configs[group_name] = positions;
}

void LayoutCalculator::clearAnalogGroupConfiguration(std::string const & group_name) {
    _analog_group_configs.erase(group_name);
}

bool LayoutCalculator::_extractGroupAndChannel(std::string const & key, std::string & group, int & channel_id) {
    channel_id = -1;
    group.clear();
    auto const pos = key.rfind('_');
    if (pos == std::string::npos || pos + 1 >= key.size()) {
        return false;
    }
    group = key.substr(0, pos);
    try {
        int const parsed = std::stoi(key.substr(pos + 1));
        channel_id = parsed > 0 ? parsed - 1 : parsed;
    } catch (...) {
        channel_id = -1;
        return false;
    }
    return true;
}

std::vector<std::string> LayoutCalculator::_orderedVisibleAnalogKeysByConfig(std::vector<std::string> const & visible_keys) const {
    // Group visible analog series by group_name
    struct Item { std::string key; std::string group; int channel; };
    std::vector<Item> items;
    items.reserve(visible_keys.size());
    for (auto const & key : visible_keys) {
        std::string group_name;
        int channel_id;
        _extractGroupAndChannel(key, group_name, channel_id);
        Item it{key, group_name, channel_id};
        items.push_back(std::move(it));
    }

    // Sort with configuration: by group; within group, if config present, by ascending y; else by channel id
    std::stable_sort(items.begin(), items.end(), [&](Item const & a, Item const & b) {
        if (a.group != b.group) return a.group < b.group;
        auto cfg_it = _analog_group_configs.find(a.group);
        if (cfg_it == _analog_group_configs.end()) {
            return a.channel < b.channel;
        }
        auto const & cfg = cfg_it->second;
        auto find_y = [&](int ch) {
            for (auto const & p : cfg) if (p.channel_id == ch) return p.y;
            return 0.0f;
        };
        float ya = find_y(a.channel);
        float yb = find_y(b.channel);
        if (ya == yb) return a.channel < b.channel;
        return ya < yb; // ascending by y so larger y get larger index (top)
    });

    std::vector<std::string> keys;
    keys.reserve(items.size());
    for (auto const & it : items) keys.push_back(it.key);
    return keys;
}
