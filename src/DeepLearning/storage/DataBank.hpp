#ifndef NEURALYZER_DATA_BANK_HPP
#define NEURALYZER_DATA_BANK_HPP

/**
 * @file DataBank.hpp
 * @brief Named storage for geometry sources and channel-encoded tensors.
 *
 * DataBank provides a DataManager-independent store for small amounts of
 * pre-encoded model input data (memory frames, reference masks, etc.).
 */

#include "channel_encoding/EncoderDispatch.hpp"// EncoderVariant, EncodingSourceVariant
#include "models_v2/TensorSlotDescriptor.hpp"  // TensorSlotDescriptor
#include "storage/DataBankEntry.hpp"           // DataBankEntry, DataBankEntryMetadata

#include "CoreGeometry/ImageSize.hpp"

#include <ATen/core/Tensor.h>// at::Tensor

#include <cstddef>
#include <optional>
#include <string>
#include <string_view>
#include <unordered_map>
#include <utility>
#include <vector>

namespace dl {

/**
 * @brief Validate a DataBank entry identifier.
 *
 * Rejects empty IDs, whitespace-only IDs, IDs containing whitespace, and
 * IDs containing ':' (reserved for legacy slot-bound cache keys).
 *
 * @return Error message on failure, or nullopt if valid
 */
[[nodiscard]] std::optional<std::string> validateEntryId(std::string_view id);

/**
 * @brief Named library for geometry sources and channel-encoded tensors.
 *
 * Entries are keyed by user-defined string IDs. Each entry may hold a raw
 * source (Mask2D, points, image pixels, line) and/or a channel-encoded
 * tensor with batch dimension 1.
 */
class DataBank {
public:
    /** Default maximum number of entries before put operations fail. */
    static constexpr std::size_t kDefaultMaxEntries = 64;

    DataBank();
    explicit DataBank(std::size_t max_entries);

    /**
     * @brief Check whether an entry exists.
     */
    [[nodiscard]] bool contains(std::string_view id) const;

    /**
     * @brief Get a copy of an entry.
     */
    [[nodiscard]] std::optional<DataBankEntry> get(std::string_view id) const;

    /**
     * @brief Get the channel-encoded tensor for an entry.
     */
    [[nodiscard]] std::optional<at::Tensor> getEncoded(std::string_view id) const;

    /**
     * @brief Get the raw source for an entry.
     */
    [[nodiscard]] std::optional<EncodingSourceVariant> getSource(
            std::string_view id) const;

    /**
     * @brief Store a raw source without encoding.
     *
     * @pre id must pass validateEntryId()
     * @post contains(id) is true and getSource(id) returns source
     */
    bool putSource(
            std::string id,
            EncodingSourceVariant source,
            DataBankEntryMetadata metadata);

    /**
     * @brief Store a channel-encoded tensor, optionally retaining the source.
     *
     * @pre id must pass validateEntryId()
     * @pre encoded must have a leading batch dimension of 1
     */
    bool putEncoded(
            std::string id,
            at::Tensor encoded,
            DataBankEntryMetadata metadata,
            std::optional<EncodingSourceVariant> source = std::nullopt);

    /**
     * @brief Encode the stored source for an entry using slot metadata.
     *
     * @pre contains(id) and getSource(id) is engaged
     * @post getEncoded(id) is engaged on success
     */
    bool encodeEntry(
            std::string_view id,
            TensorSlotDescriptor const & slot,
            EncoderVariant const & encoder_params);

    /**
     * @brief Remove a single entry.
     */
    void remove(std::string_view id);

    /**
     * @brief Remove all entries.
     */
    void clear();

    /**
     * @brief List all entry IDs.
     */
    [[nodiscard]] std::vector<std::string> ids() const;

    /**
     * @brief Number of stored entries.
     */
    [[nodiscard]] std::size_t size() const;

    /**
     * @brief Maximum allowed entries.
     */
    [[nodiscard]] std::size_t maxEntries() const;

    /**
     * @brief Tensor shape for preview, or empty if missing or not encoded.
     */
    [[nodiscard]] std::vector<int64_t> encodedShape(std::string_view id) const;

    /**
     * @brief Min/max of encoded tensor values, or {0,0} if missing.
     */
    [[nodiscard]] std::pair<float, float> encodedRange(std::string_view id) const;

private:
    [[nodiscard]] bool canAcceptNewEntry(std::string_view id) const;

    std::size_t _max_entries;
    std::unordered_map<std::string, DataBankEntry> _entries;
};

}// namespace dl

#endif// NEURALYZER_DATA_BANK_HPP
