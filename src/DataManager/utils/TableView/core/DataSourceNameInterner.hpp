#ifndef DATA_SOURCE_NAME_INTERNER_HPP
#define DATA_SOURCE_NAME_INTERNER_HPP

#include <cstdint>
#include <mutex>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

using DataSourceId = std::uint32_t;

/**
 * @brief App-level string interner for data source names.
 *
 * Stores each distinct name once and hands out compact integer ids for
 * per-row usage. Thread-safe for concurrent reads/writes.
 */
class DataSourceNameInterner {
public:
    static auto instance() -> DataSourceNameInterner& {
        static DataSourceNameInterner inst;
        return inst;
    }

    auto intern(std::string const& name) -> DataSourceId {
        std::lock_guard<std::mutex> const lock(mutex_);
        auto it = name_to_id_.find(name);
        if (it != name_to_id_.end()) {
            return it->second;
        }
        auto const id = static_cast<DataSourceId>(id_to_name_.size());
        id_to_name_.push_back(name);
        name_to_id_.emplace(id_to_name_.back(), id);
        return id;
    }

    auto intern(std::string_view name) -> DataSourceId {
        return intern(std::string{name});
    }

    auto nameOf(DataSourceId id) const -> std::string const& {
        std::lock_guard<std::mutex> const lock(mutex_);
        return id_to_name_.at(static_cast<size_t>(id));
    }

    auto contains(std::string const& name) const -> bool {
        std::lock_guard<std::mutex> const lock(mutex_);
        return name_to_id_.find(name) != name_to_id_.end();
    }

    DataSourceNameInterner(DataSourceNameInterner const&) = delete;
    auto operator=(DataSourceNameInterner const&) -> DataSourceNameInterner& = delete;
    DataSourceNameInterner(DataSourceNameInterner&&) = delete;
    auto operator=(DataSourceNameInterner&&) -> DataSourceNameInterner& = delete;

private:
    DataSourceNameInterner() = default;
    ~DataSourceNameInterner() = default;

    mutable std::mutex mutex_;
    std::unordered_map<std::string, DataSourceId> name_to_id_;
    std::vector<std::string> id_to_name_;
};

#endif // DATA_SOURCE_NAME_INTERNER_HPP


