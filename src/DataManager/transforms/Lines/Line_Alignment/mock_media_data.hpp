#ifndef MOCK_MEDIA_DATA_HPP
#define MOCK_MEDIA_DATA_HPP

#include "CoreGeometry/ImageSize.hpp"
#include "Media/Media_Data.hpp"

#include <type_traits>
#include <vector>

/**
 * @brief Mock MediaData subclass for testing and benchmarking
 */
class MockMediaData : public MediaData {
public:
    /**
     * @brief Constructor for MockMediaData
     * @param bit_depth The bit depth for the mock data
     */
    explicit MockMediaData(BitDepth bit_depth = BitDepth::Bit8) {
        setBitDepth(bit_depth);
    }

    /**
     * @brief Get the media type
     * @return MediaType::Images
     */
    MediaType getMediaType() const override {
        return MediaType::Images;
    }

    /**
     * @brief Add an 8-bit image to the mock media data
     * 
     * @param image_data The image data as uint8_t vector
     * @param image_size The image dimensions
     */
    void addImage8(std::vector<uint8_t> const & image_data, ImageSize const & image_size) {
        setBitDepth(BitDepth::Bit8);
        _stored_image_8bit = image_data;
        updateWidth(image_size.width);
        updateHeight(image_size.height);
        setTotalFrameCount(1);// Simple mock - only one frame
    }

    /**
     * @brief Add a 32-bit float image to the mock media data
     * 
     * @param image_data The image data as float vector
     * @param image_size The image dimensions
     */
    void addImage32(std::vector<float> const & image_data, ImageSize const & image_size) {
        setBitDepth(BitDepth::Bit32);
        _stored_image_32bit = image_data;
        updateWidth(image_size.width);
        updateHeight(image_size.height);
        setTotalFrameCount(1);// Simple mock - only one frame
    }

    /**
     * @brief Add an image based on the current bit depth
     * @tparam T The pixel type (uint8_t or float)
     * @param image_data The image data
     * @param image_size The image dimensions
     */
    template<typename T>
    void addImage(std::vector<T> const & image_data, ImageSize const & image_size) {
        if constexpr (std::is_same_v<T, uint8_t>) {
            addImage8(image_data, image_size);
        } else if constexpr (std::is_same_v<T, float>) {
            addImage32(image_data, image_size);
        }
    }

protected:
    /**
     * @brief Load media from file (no-op for mock data)
     * @param name The file name (unused)
     */
    void doLoadMedia(std::string const & name) override {
        static_cast<void>(name);
    }

    /**
     * @brief Load a specific frame (loads stored image data)
     * @param frame_id The frame ID (unused, only frame 0 exists)
     */
    void doLoadFrame(int frame_id) override {
        static_cast<void>(frame_id);// We only have one frame (frame 0)

        if (is8Bit() && !_stored_image_8bit.empty()) {
            setRawData(_stored_image_8bit);
        } else if (is32Bit() && !_stored_image_32bit.empty()) {
            setRawData(_stored_image_32bit);
        }
    }

private:
    std::vector<uint8_t> _stored_image_8bit;///< Stored 8-bit image data
    std::vector<float> _stored_image_32bit; ///< Stored 32-bit image data
};

#endif// MOCK_MEDIA_DATA_HPP
