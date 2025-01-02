
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

torch::Tensor convert_image_vec_to_tensor(std::vector<uint8_t> image, int height, int width, int channels=3)
{
    auto image_tensor = dl::create_tensor_from_gray8(image, height, width);
    if (channels > 1) {
        image_tensor = image_tensor.repeat({1, channels, 1, 1});
    }

    auto data_input = torch::nn::functional::interpolate(
        image_tensor,
        torch::nn::functional::InterpolateFuncOptions().size(std::vector<int64_t>({256,256}))
            .mode(torch::kBilinear)
            .antialias(true)
            .align_corners(false));

    data_input = data_input.to(torch::kFloat32).div(255).contiguous().to(device);

    return data_input;
}

void SCM::process_frame(std::vector<uint8_t>& image, int height, int width) {

    device = dl::get_device();

    if (!module) {
        module = dl::load_torchscript_model("/home/wanglab/Desktop/efficientvit_pytorch_cuda2.pt", device);
    }

    module->to(device);

    if (_memory.empty())
    {
        std::cout << "Currently no frames in memory. Please select some" << std::endl;
        return;
    }

    auto image_tensor = convert_image_vec_to_tensor(image, height, width);

    auto memory_frame_tensor_list = std::vector<torch::Tensor>(memory_frames + 1);
    auto memory_label_tensor_list = std::vector<torch::Tensor>(memory_frames + 1);
    auto mask_vector = std::vector<float>(memory_frames + 1);
    for (auto & memory_pair : _memory) {
        memory_frame_tensor_list[memory_pair.first] = convert_image_vec_to_tensor(memory_pair.second.memory_frame, height, width);
        memory_label_tensor_list[memory_pair.first] = convert_image_vec_to_tensor(memory_pair.second.memory_label, height, width, 1);
        mask_vector[memory_pair.first] = 1.0;
    }

    for (auto i = 0; i < mask_vector.size(); i ++)
    {
        if (mask_vector[i] == 0.0) {
            memory_frame_tensor_list[i] = torch::zeros({1, 3, 256, 256},torch::kFloat32).to(device);
            memory_label_tensor_list[i] = torch::zeros({1, 1, 256, 256}, torch::kFloat32).to(device);
        }
    }

    auto memory_frame_tensor = torch::stack(memory_frame_tensor_list,1);
    auto memory_label_tensor = torch::stack(memory_label_tensor_list,1);
    auto mask_tensor = torch::tensor(mask_vector).to(device);
    mask_tensor = mask_tensor.unsqueeze(0);


    //data_input = data_input.repeat({100, 1, 1, 1});

    torch::NoGradGuard no_grad;

    //torch::jit::setGraphExecutorOptimize(false);
    auto output = module->forward({image_tensor, memory_frame_tensor, memory_label_tensor, mask_tensor}).toTensor();
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
