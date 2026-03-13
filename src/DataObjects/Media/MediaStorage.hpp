#ifndef WHISKERTOOLBOX_MEDIA_STORAGE_HPP
#define WHISKERTOOLBOX_MEDIA_STORAGE_HPP

#include <vector>
#include <variant>
#include <cstdint>

namespace MediaStorage {

/**
 * @brief Type alias for 8-bit image data
 */
using ImageData8 = std::vector<uint8_t>;

/**
 * @brief Type alias for 32-bit float image data (normalized to 0-255 range)
 */
using ImageData32 = std::vector<float>;

/**
 * @brief Variant type that can hold either 8-bit or 32-bit image data
 * 
 * This is the unified storage type used throughout the media processing pipeline.
 * It eliminates the need for conversions between different variant types in
 * MediaData and ImageProcessor classes.
 * 
 * Index 0: ImageData8 (std::vector<uint8_t>)
 * Index 1: ImageData32 (std::vector<float>)
 */
using ImageDataVariant = std::variant<ImageData8, ImageData32>;

/**
 * @brief Enumeration for bit depth types
 */
enum class BitDepth {
    Bit8,   ///< 8-bit unsigned integer data
    Bit32   ///< 32-bit float data (normalized to 0-255 range)
};

/**
 * @brief Helper function to get BitDepth from variant index
 * @param variant_index The index from std::variant::index()
 * @return Corresponding BitDepth enum value
 */
constexpr BitDepth getBitDepthFromIndex(size_t variant_index) {
    return variant_index == 0 ? BitDepth::Bit8 : BitDepth::Bit32;
}

/**
 * @brief Helper function to get variant index from BitDepth
 * @param depth The BitDepth enum value
 * @return Corresponding variant index (0 for Bit8, 1 for Bit32)
 */
constexpr size_t getIndexFromBitDepth(BitDepth depth) {
    return depth == BitDepth::Bit8 ? 0 : 1;
}

/**
 * @brief Check if variant contains 8-bit data
 * @param data The ImageDataVariant to check
 * @return true if contains 8-bit data, false otherwise
 */
inline bool is8Bit(ImageDataVariant const& data) {
    return data.index() == 0;
}

/**
 * @brief Check if variant contains 32-bit data
 * @param data The ImageDataVariant to check
 * @return true if contains 32-bit data, false otherwise
 */
inline bool is32Bit(ImageDataVariant const& data) {
    return data.index() == 1;
}

/**
 * @brief Get the bit depth of the variant data
 * @param data The ImageDataVariant to check
 * @return BitDepth enum value
 */
inline BitDepth getBitDepth(ImageDataVariant const& data) {
    return getBitDepthFromIndex(data.index());
}

} // namespace MediaStorage

#endif // WHISKERTOOLBOX_MEDIA_STORAGE_HPP
