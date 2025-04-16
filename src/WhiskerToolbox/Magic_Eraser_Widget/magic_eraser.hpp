#ifndef MAGIC_ERASER_HPP
#define MAGIC_ERASER_HPP

#include "DataManager/ImageSize/ImageSize.hpp"

#include <cstdint>
#include <vector>

namespace cv {
class Mat;
}

class MagicEraser {
public:
    MagicEraser() = default;
    std::vector<uint8_t> applyMagicEraser(std::vector<uint8_t> & image, ImageSize image_size, std::vector<uint8_t> & mask);

private:
    cv::Mat _createBackgroundImage(std::vector<uint8_t> const & image, ImageSize image_size);
};

std::vector<uint8_t> apply_magic_eraser(std::vector<uint8_t> & image, ImageSize image_size, std::vector<uint8_t> & mask);


#endif// MAGIC_ERASER_HPP
