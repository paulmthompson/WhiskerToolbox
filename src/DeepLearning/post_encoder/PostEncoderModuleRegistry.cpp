/**
 * @file PostEncoderModuleRegistry.cpp
 * @brief Implementation of PostEncoderModuleRegistry.
 */

#include "PostEncoderModuleRegistry.hpp"

#include <spdlog/spdlog.h>

#include <algorithm>
#include <cassert>

namespace dl {

namespace {

[[nodiscard]] bool isNoneKey(std::string const & key) {
    return key.empty() || key == "none";
}

}// namespace

PostEncoderModuleRegistry & PostEncoderModuleRegistry::instance() {
    static PostEncoderModuleRegistry registry;
    return registry;
}

void PostEncoderModuleRegistry::registerModule(PostEncoderModuleEntry entry) {
    assert(!entry.metadata.key.empty() && "PostEncoder module key must not be empty");
    assert(entry.factory && "PostEncoder module factory must not be null");

    auto const & key = entry.metadata.key;
    if (_entries.contains(key)) {
        spdlog::warn(
                "PostEncoderModuleRegistry: module '{}' is already registered; skipping "
                "duplicate",
                key);
        return;
    }

    _entries.emplace(key, std::move(entry));
}

std::vector<std::string> PostEncoderModuleRegistry::moduleKeys() const {
    std::vector<std::string> keys;
    keys.reserve(_entries.size());
    for (auto const & [key, _]: _entries) {
        keys.push_back(key);
    }
    std::sort(keys.begin(), keys.end());
    return keys;
}

bool PostEncoderModuleRegistry::hasModule(std::string const & key) const {
    return _entries.contains(key);
}

std::optional<PostEncoderModuleMetadata>
PostEncoderModuleRegistry::getMetadata(std::string const & key) const {
    auto const it = _entries.find(key);
    if (it == _entries.end()) {
        return std::nullopt;
    }
    return it->second.metadata;
}

std::optional<ParameterSchema>
PostEncoderModuleRegistry::getSchema(std::string const & key) const {
    auto const it = _entries.find(key);
    if (it == _entries.end()) {
        return std::nullopt;
    }
    return it->second.schema;
}

std::unique_ptr<PostEncoderModule>
PostEncoderModuleRegistry::create(std::string const & key,
                                std::string const & params_json,
                                ImageSize source_image_size) const {
    if (isNoneKey(key)) {
        return nullptr;
    }

    auto const it = _entries.find(key);
    if (it == _entries.end()) {
        spdlog::debug(
                "PostEncoderModuleRegistry: unknown module key '{}'",
                key);
        return nullptr;
    }

    return it->second.factory(params_json, source_image_size);
}

bool PostEncoderModuleRegistry::collapsesSpatialDims(std::string const & key) const {
    if (isNoneKey(key)) {
        return false;
    }

    auto const it = _entries.find(key);
    if (it == _entries.end()) {
        return false;
    }
    return it->second.metadata.collapses_spatial_dims;
}

std::optional<std::vector<int64_t>>
PostEncoderModuleRegistry::outputShape(std::string const & key,
                                     std::vector<int64_t> const & input_shape,
                                     std::string const & params_json,
                                     ImageSize source_image_size) const {
    auto module = create(key, params_json, source_image_size);
    if (!module) {
        return std::nullopt;
    }
    return module->outputShape(input_shape);
}

}// namespace dl
