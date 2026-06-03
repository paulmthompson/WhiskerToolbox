
#include "PipelineValueStore.hpp"

#include <format>

namespace WhiskerToolbox::Transforms::V2 {

std::optional<std::string> PipelineValueStore::getJson(std::string const & key) const {
    auto it = values_.find(key);
    if (it == values_.end()) {
        return std::nullopt;
    }

    return std::visit([](auto const & val) -> std::string {
        using T = std::decay_t<decltype(val)>;
        if constexpr (std::is_same_v<T, float>) {
            // Use sufficient precision for float
            return std::format("{}", val);
        } else if constexpr (std::is_same_v<T, int64_t>) {
            return std::format("{}", val);
        } else if constexpr (std::is_same_v<T, std::string>) {
            // JSON-encode string with quotes
            return std::format("\"{}\"", val);
        }
    },
                      it->second);
}

std::optional<float> PipelineValueStore::getFloat(std::string const & key) const {
    auto it = values_.find(key);
    if (it == values_.end()) {
        return std::nullopt;
    }

    return std::visit([](auto const & val) -> std::optional<float> {
        using T = std::decay_t<decltype(val)>;
        if constexpr (std::is_same_v<T, float>) {
            return val;
        } else if constexpr (std::is_same_v<T, int64_t>) {
            return static_cast<float>(val);
        } else {
            return std::nullopt;// string cannot convert to float
        }
    },
                      it->second);
}

std::optional<int64_t> PipelineValueStore::getInt(std::string const & key) const {
    auto it = values_.find(key);
    if (it == values_.end()) {
        return std::nullopt;
    }

    return std::visit([](auto const & val) -> std::optional<int64_t> {
        using T = std::decay_t<decltype(val)>;
        if constexpr (std::is_same_v<T, int64_t>) {
            return val;
        } else if constexpr (std::is_same_v<T, float>) {
            return static_cast<int64_t>(val);
        } else {
            return std::nullopt;// string cannot convert to int
        }
    },
                      it->second);
}

std::optional<std::string> PipelineValueStore::getString(std::string const & key) const {
    auto it = values_.find(key);
    if (it == values_.end()) {
        return std::nullopt;
    }

    if (auto const * str = std::get_if<std::string>(&it->second)) {
        return *str;
    }
    return std::nullopt;
}

}// namespace WhiskerToolbox::Transforms::V2