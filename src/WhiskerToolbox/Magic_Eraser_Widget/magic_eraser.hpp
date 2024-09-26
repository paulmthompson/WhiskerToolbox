#ifndef MAGIC_ERASER_HPP
#define MAGIC_ERASER_HPP


#include <cstdint>
#include <vector>

namespace cv {
    class Mat;
}

class MagicEraser
{
public:
    MagicEraser();
    std::vector<uint8_t> applyMagicEraser(std::vector<uint8_t> & image, int width, int height, std::vector<uint8_t> & mask);

private:
    cv::Mat _createBackgroundImage(std::vector<uint8_t> const & image, int width, int height);
};

std::vector<uint8_t> apply_magic_eraser(std::vector<uint8_t> & image, int width, int height, std::vector<uint8_t> & mask);




#endif // MAGIC_ERASER_HPP
