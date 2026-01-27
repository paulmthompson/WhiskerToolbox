#include "SpikeSorterConfigLoader.hpp"

#include <algorithm>
#include <sstream>

std::vector<ChannelPosition> parseSpikeSorterConfig(std::string const & text) {
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
            continue;  // skip header row
        }
        
        std::istringstream ls(line);
        int row = 0;
        int ch = 0;
        float x = 0.0f;
        float y = 0.0f;
        
        if (!(ls >> row >> ch >> x >> y)) {
            continue;
        }
        
        // SpikeSorter is 1-based; convert to 0-based for our program
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

std::vector<std::string> orderKeysBySpikeSorterConfig(
        std::vector<std::string> const & keys,
        SpikeSorterConfigMap const & configs) {
    
    // Internal helper struct for sorting
    struct Item {
        std::string key;
        std::string group;
        int channel;
    };
    
    std::vector<Item> items;
    items.reserve(keys.size());
    
    for (auto const & key : keys) {
        std::string group_name;
        int channel_id;
        (void)extractGroupAndChannelFromKey(key, group_name, channel_id);
        Item it{key, group_name, channel_id};
        items.push_back(std::move(it));
    }

    // Sort with configuration: by group; within group, if config present, by ascending y; else by channel id
    std::stable_sort(items.begin(), items.end(), [&configs](Item const & a, Item const & b) {
        if (a.group != b.group) {
            return a.group < b.group;
        }
        
        auto cfg_it = configs.find(a.group);
        if (cfg_it == configs.end()) {
            return a.channel < b.channel;
        }
        
        auto const & cfg = cfg_it->second;
        auto find_y = [&cfg](int ch) {
            for (auto const & p : cfg) {
                if (p.channel_id == ch) {
                    return p.y;
                }
            }
            return 0.0f;
        };
        
        float ya = find_y(a.channel);
        float yb = find_y(b.channel);
        
        if (ya == yb) {
            return a.channel < b.channel;
        }
        return ya < yb;  // ascending by y so larger y get larger index (top)
    });

    std::vector<std::string> result;
    result.reserve(items.size());
    for (auto const & it : items) {
        result.push_back(it.key);
    }
    
    return result;
}
