#ifndef DATA_MANAGER_CONTAINER_HPP
#define DATA_MANAGER_CONTAINER_HPP

#include <map>
#include <vector>
#include <unordered_map>

template<typename K, typename... T>
std::vector<K> get_keys(std::unordered_map<K, T...>& map)
{
    //https://stackoverflow.com/questions/8483985/obtaining-list-of-keys-and-values-from-unordered-map
    std::vector<K> keys;
    keys.reserve(map.size());
    for (auto kv : map) {
        keys.push_back(kv.first);
    }
    return keys;
}

template<typename K, typename... T>
std::vector<K> get_keys(std::map<K, T...>& map)
{
    //https://stackoverflow.com/questions/8483985/obtaining-list-of-keys-and-values-from-unordered-map
    std::vector<K> keys;
    keys.reserve(map.size());
    for (auto kv : map) {
        keys.push_back(kv.first);
    }
    return keys;
}


#endif // CONTAINER_HPP
