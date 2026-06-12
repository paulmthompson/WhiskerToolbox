#ifndef WHISKERTOOLBOX_DATA_BANK_ENTRY_HPP
#define WHISKERTOOLBOX_DATA_BANK_ENTRY_HPP

/**
 * @file DataBankEntry.hpp
 * @brief Entry and metadata types for dl::DataBank named storage.
 */

#include "channel_encoding/EncoderDispatch.hpp"

#include "CoreGeometry/ImageSize.hpp"

#include <ATen/core/Tensor.h>

#include <optional>
#include <string>

namespace dl {

/**
 * @brief Provenance and encoding metadata for a DataBank entry.
 *
 * When an entry is captured from DataManager, @p data_key and @p captured_frame
 * together identify the source object and frame so the same data can be
 * re-fetched later without relying on the cached geometry copy alone.
 */
struct DataBankEntryMetadata {
    /** Optional human-readable label for UI display. */
    std::string label;
    /** DataManager object key the source was captured from (empty if unknown). */
    std::string data_key;
    /** Frame index in the source object's timebase (-1 if unknown). */
    int captured_frame = -1;
    /** Original image dimensions used for geometry coordinate scaling. */
    ImageSize source_image_size{};
    /** Encoder factory name used for encoding (e.g. "Mask2DEncoder"). */
    std::string encoder_factory_name;
};

/**
 * @brief A single named entry in a DataBank.
 *
 * Stores optional raw source geometry/image data and an optional
 * channel-encoded tensor ready for model input (batch dimension = 1).
 */
struct DataBankEntry {
    std::optional<EncodingSourceVariant> source;
    std::optional<at::Tensor> encoded;
    DataBankEntryMetadata metadata;
};

}// namespace dl

#endif// WHISKERTOOLBOX_DATA_BANK_ENTRY_HPP
