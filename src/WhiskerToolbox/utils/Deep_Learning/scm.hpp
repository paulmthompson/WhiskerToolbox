#ifndef SCM_HPP
#define SCM_HPP

#include <memory>
#include <string>
#include <vector>

namespace torch::jit {class Module;}

namespace dl {


class SCM {
public:
    SCM();
    virtual ~SCM() {};
    void load_model();
    void process_frame(std::vector<uint8_t>& image, int height, int width);

private:

    std::shared_ptr<torch::jit::Module> module {nullptr};
    std::string module_path;
};

}
#endif // SCM_HPP
