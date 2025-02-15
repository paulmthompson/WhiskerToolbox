#ifndef EFFICIENTSAM_HPP
#define EFFICIENTSAM_HPP

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

    std::vector<uint8_t> process_frame(std::vector<uint8_t> const & image, int const height, int const width, int const x, int const y);

private:
    std::shared_ptr<torch::jit::Module> _module {nullptr};
    std::string _module_path;
};


}

#endif // EFFICIENTSAM_HPP
