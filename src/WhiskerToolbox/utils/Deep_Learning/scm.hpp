#ifndef SCM_HPP
#define SCM_HPP

#include "DataManager/Points/points.hpp"
//#include "DataManager/Tensors/Tensor_Data.hpp"

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
    std::vector<Point2D<float>> process_frame(std::vector<uint8_t>& image, int height, int width);
    void add_memory_frame(std::vector<uint8_t> memory_frame, std::vector<uint8_t> memory_label);
    void add_origin(float x, float y) {_x = x /  _width * 256; _y = y / _height * 256;};
    void add_height_width(int height, int width) {_height = height; _width = width;};

private:
    void _create_memory_tensors();

    std::shared_ptr<torch::jit::Module> module {nullptr};
    std::unique_ptr<memory_encoder_tensors> _memory_tensors {nullptr};
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
