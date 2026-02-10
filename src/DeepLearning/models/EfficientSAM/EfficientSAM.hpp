#ifndef EFFICIENTSAM_HPP
#define EFFICIENTSAM_HPP

#include "CoreGeometry/ImageSize.hpp"

#include <memory>
#include <string>
#include <vector>

namespace torch::jit {class Module;}

namespace dl {

class EfficientSAM {


public:
    EfficientSAM();
    ~EfficientSAM();

    void load_model();

    std::vector<uint8_t> process_frame(std::vector<uint8_t> const & image, ImageSize image_size, int x, int y);

private:
    std::shared_ptr<torch::jit::Module> _module {nullptr};
    std::string _module_path;
};


}

#endif // EFFICIENTSAM_HPP
