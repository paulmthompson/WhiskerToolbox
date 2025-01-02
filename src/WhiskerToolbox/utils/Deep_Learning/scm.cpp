
#include "scm.hpp"

#include "torch_helpers.hpp"
#include <torch/torch.h>

#include <iostream>

namespace dl {

torch::Device device(torch::kCPU);


SCM::SCM()
{
    module_path = "/home/wanglab/Desktop/efficientvit_pytorch_cuda2.pt";

    load_model();
}

void SCM::load_model()
{
    if (!module) {
        module = dl::load_torchscript_model(module_path, device);
    }
}

void SCM::process_frame(std::vector<uint8_t>& image, int height, int width) {

    device = dl::get_device();

    if (!module) {
        module = dl::load_torchscript_model("/home/wanglab/Desktop/efficientvit_pytorch_cuda2.pt", device);
    }

    if (_memory.empty())
    {
        std::cout << "Currently no frames in memory. Please select some" << std::endl;
        return;
    }

    auto tensor = dl::create_tensor_from_gray8(image, height, width);
    tensor = tensor.repeat({1, 3, 1, 1});

    auto data_input = torch::nn::functional::interpolate(
        tensor,
        torch::nn::functional::InterpolateFuncOptions().size(std::vector<int64_t>({256,256}))
            .mode(torch::kBilinear)
            .antialias(true)
            .align_corners(false));

    data_input = data_input.to(torch::kFloat32).div(255).contiguous().to(device);

    //data_input = data_input.repeat({100, 1, 1, 1});

    torch::NoGradGuard no_grad;

    //torch::jit::setGraphExecutorOptimize(false);
    auto output = module->forward({data_input}).toTensor();
}

void SCM::add_memory_frame(std::vector<uint8_t> memory_frame, std::vector<uint8_t> memory_label)
{
    int key_index;

    if (_memory.empty())
    {
        key_index = 0;
    } else if (_memory.rbegin()->first >= memory_frames)
    {
        key_index = _memory.rbegin()->first;
    } else {
        key_index = _memory.rbegin()->first + 1;
    }

    _memory[key_index] = memory_frame_pair{memory_frame, memory_label};

    std::cout << "memory frame added at " << key_index << std::endl;
}


}
