#ifndef SCM_HPP
#define SCM_HPP

#include "DataManager/Points/Point_Data.hpp"

#include <map>
#include <memory>
#include <string>
#include <vector>

namespace torch::jit {class Module;}

namespace dl {

struct memory_frame_pair {
    std::vector<uint8_t> memory_frame;
    std::vector<uint8_t> memory_label;
};

class SCM {
public:
    SCM();
    virtual ~SCM() {};
    void load_model();
    std::vector<Point2D<float>> process_frame(std::vector<uint8_t>& image, int height, int width);
    void add_memory_frame(std::vector<uint8_t> memory_frame, std::vector<uint8_t> memory_label);
    void add_origin(float x, float y) {_x = x / 256; _y = y / 256;};

private:

    std::shared_ptr<torch::jit::Module> module {nullptr};
    std::string module_path;
    std::map<int, memory_frame_pair> _memory;
    int memory_frames {4};
    float _x {0};
    float _y {0};
};

}
#endif // SCM_HPP
