/**
 * @file DataBank.cpp
 * @brief Implementation of dl::DataBank named storage.
 */

#include "DataBank.hpp"

#include "storage/DataBankEncode.hpp"

#include <spdlog/spdlog.h>

#include <algorithm>
#include <cassert>
#include <cctype>
#include <cstdint>
#include <utility>

namespace dl {

namespace {

[[nodiscard]] bool hasOnlyWhitespace(std::string_view id) {
    return std::ranges::all_of(id, [](unsigned char c) { return std::isspace(c) != 0; });
}

[[nodiscard]] bool hasInteriorWhitespace(std::string_view id) {
    return std::ranges::any_of(id, [](unsigned char c) { return std::isspace(c) != 0; });
}

}// namespace

std::optional<std::string> validateEntryId(std::string_view id) {
    if (id.empty()) {
        return std::string{"DataBank entry ID must not be empty"};
    }
    if (hasOnlyWhitespace(id)) {
        return std::string{"DataBank entry ID must not be whitespace only"};
    }
    if (hasInteriorWhitespace(id)) {
        return std::string{"DataBank entry ID must not contain whitespace"};
    }
    if (id.find(':') != std::string_view::npos) {
        return std::string{"DataBank entry ID must not contain ':'"};
    }
    return std::nullopt;
}

DataBank::DataBank()
    : DataBank(kDefaultMaxEntries) {
}

DataBank::DataBank(std::size_t max_entries)
    : _max_entries(max_entries > 0 ? max_entries : kDefaultMaxEntries) {
}

bool DataBank::contains(std::string_view id) const {
    return _entries.contains(std::string{id});
}

std::optional<DataBankEntry> DataBank::get(std::string_view id) const {
    auto const it = _entries.find(std::string{id});
    if (it == _entries.end()) {
        return std::nullopt;
    }
    return it->second;
}

std::optional<at::Tensor> DataBank::getEncoded(std::string_view id) const {
    auto const entry = get(id);
    if (!entry || !entry->encoded) {
        return std::nullopt;
    }
    return *entry->encoded;
}

std::optional<EncodingSourceVariant> DataBank::getSource(std::string_view id) const {
    auto const entry = get(id);
    if (!entry || !entry->source) {
        return std::nullopt;
    }
    return *entry->source;
}

bool DataBank::canAcceptNewEntry(std::string_view id) const {
    if (contains(id)) {
        return true;
    }
    if (_entries.size() >= _max_entries) {
        spdlog::error(
                "DataBank: cannot add entry '{}' — limit of {} entries reached",
                id,
                _max_entries);
        return false;
    }
    return true;
}

bool DataBank::putSource(
        std::string id,
        EncodingSourceVariant source,
        DataBankEntryMetadata metadata) {

    if (auto const err = validateEntryId(id)) {
        spdlog::error("DataBank::putSource: {}", *err);
        return false;
    }
    if (!canAcceptNewEntry(id)) {
        return false;
    }

    auto & entry = _entries[id];
    entry.source = std::move(source);
    entry.metadata = std::move(metadata);
    return true;
}

bool DataBank::putEncoded(
        std::string id,
        at::Tensor encoded,
        DataBankEntryMetadata metadata,
        std::optional<EncodingSourceVariant> source) {

    if (auto const err = validateEntryId(id)) {
        spdlog::error("DataBank::putEncoded: {}", *err);
        return false;
    }
    if (!encoded.defined() || encoded.dim() < 1 || encoded.size(0) != 1) {
        spdlog::error(
                "DataBank::putEncoded: tensor for '{}' must be defined with batch size 1",
                id);
        return false;
    }
    if (!canAcceptNewEntry(id)) {
        return false;
    }

    auto & entry = _entries[id];
    entry.encoded = std::move(encoded);
    entry.metadata = std::move(metadata);
    if (source) {
        entry.source = std::move(*source);
    }
    return true;
}

bool DataBank::encodeEntry(
        std::string_view id,
        TensorSlotDescriptor const & slot,
        EncoderVariant const & encoder_params) {

    auto const id_str = std::string{id};
    auto const it = _entries.find(id_str);
    if (it == _entries.end()) {
        spdlog::error("DataBank::encodeEntry: unknown entry '{}'", id);
        return false;
    }
    if (!it->second.source) {
        spdlog::error("DataBank::encodeEntry: entry '{}' has no source data", id);
        return false;
    }

    auto encoded = encodeSourceToTensor(
            *it->second.source,
            slot,
            encoder_params,
            it->second.metadata.source_image_size);
    if (!encoded) {
        spdlog::error("DataBank::encodeEntry: encoding failed for '{}'", id);
        return false;
    }

    it->second.encoded = std::move(*encoded);
    it->second.metadata.encoder_factory_name = encoderFactoryName(encoder_params);
    return true;
}

void DataBank::remove(std::string_view id) {
    _entries.erase(std::string{id});
}

void DataBank::clear() {
    _entries.clear();
}

std::vector<std::string> DataBank::ids() const {
    std::vector<std::string> result;
    result.reserve(_entries.size());
    for (auto const & [key, _]: _entries) {
        result.push_back(key);
    }
    std::ranges::sort(result);
    return result;
}

std::size_t DataBank::size() const {
    return _entries.size();
}

std::size_t DataBank::maxEntries() const {
    return _max_entries;
}

std::vector<int64_t> DataBank::encodedShape(std::string_view id) const {
    auto const tensor = getEncoded(id);
    if (!tensor) {
        return {};
    }
    auto const sizes = tensor->sizes();
    return {sizes.begin(), sizes.end()};
}

std::pair<float, float> DataBank::encodedRange(std::string_view id) const {
    auto const tensor = getEncoded(id);
    if (!tensor) {
        return {0.0f, 0.0f};
    }
    return {tensor->min().item<float>(), tensor->max().item<float>()};
}

}// namespace dl
