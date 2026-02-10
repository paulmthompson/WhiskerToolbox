#ifndef SCM_HPP
#define SCM_HPP


#include "CoreGeometry/ImageSize.hpp"
#include "CoreGeometry/masks.hpp"
#include "CoreGeometry/points.hpp"

#include <map>
#include <memory>
#include <string>
#include <vector>

namespace torch::jit {class Module;}

namespace dl {

struct memory_encoder_tensors;

struct memory_frame_pair {
    std::vector<uint8_t> memory_frame;
    std::vector<uint8_t> memory_label;
};

class SCM {
public:
    SCM();
    ~SCM();
    void load_model();
    Mask2D process_frame(std::vector<uint8_t>& image, ImageSize image_size);
    void add_memory_frame(std::vector<uint8_t> memory_frame, std::vector<uint8_t> memory_label);
    void add_origin(float x, float y) {
        _x = x / static_cast<float>(_width) * 256;
        _y = y / static_cast<float>(_height) * 256;
    };
    void add_height_width(ImageSize const image_size) {_height = image_size.height; _width = image_size.width;};

private:
    void _create_memory_tensors();

    std::shared_ptr<torch::jit::Module> module {nullptr};
    std::unique_ptr<memory_encoder_tensors> _memory_tensors;
    std::string module_path;
    std::map<int, memory_frame_pair> _memory;
    int memory_frames {4};
    float _x {0};
    float _y {0};
    int _height {256};
    int _width {256};
};

}
#endif // SCM_HPP
