
#include "scm.hpp"
#include "utils/Image_Processing/skeletonize.hpp"
#include "utils/Image_Processing/order_line.hpp"
#include "DataManager/Points/points.hpp"

#include "torch_helpers.hpp"
#include <torch/torch.h>

#include <chrono>
#include <iostream>

namespace dl {

torch::Device device(torch::kCPU);

struct memory_encoder_tensors {
    torch::Tensor memory_frame_tensor;
    torch::Tensor memory_label_tensor;
    torch::Tensor mask_tensor;

    memory_encoder_tensors() :
        memory_frame_tensor{torch::zeros({0})},
        memory_label_tensor{torch::zeros({0})},
        mask_tensor{torch::zeros({0})} {}
};

SCM::SCM()
{
    module_path = "C:/Users/wanglab/Desktop/efficientvit_pytorch_cuda2.pt";

    load_model();

    _memory_tensors = std::make_unique<memory_encoder_tensors>();
}

SCM::~SCM() {}

void SCM::load_model()
{
    if (!module) {
        module = dl::load_torchscript_model(module_path, device);
    }
}

torch::Tensor convert_image_vec_to_tensor(std::vector<uint8_t> image, int height, int width, int channels=3, bool smooth=false)
{
    auto image_tensor = dl::create_tensor_from_gray8(image, height, width);
    if (channels > 1) {
        image_tensor = image_tensor.repeat({1, channels, 1, 1});
    }
    if (smooth)
    {
        auto filter_size = 5;
        auto filter = torch::ones({filter_size, filter_size}, torch::kFloat32);
        image_tensor = image_tensor.to(torch::kFloat32).div(255);
        image_tensor = torch::conv2d(image_tensor, filter.unsqueeze(0).unsqueeze(0), {}, 1, filter_size / 2 );
        image_tensor = image_tensor.mul(255).to(torch::kByte);
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

void SCM::_create_memory_tensors()
{
    auto memory_frame_tensor_list = std::vector<torch::Tensor>(memory_frames + 1);
    auto memory_label_tensor_list = std::vector<torch::Tensor>(memory_frames + 1);
    auto mask_vector = std::vector<float>(memory_frames + 1);
    for (auto & memory_pair : _memory) {
        memory_frame_tensor_list[memory_pair.first] = convert_image_vec_to_tensor(memory_pair.second.memory_frame, _height, _width);

        auto memory_label = convert_image_vec_to_tensor(memory_pair.second.memory_label, _height, _width, 1, true);
        memory_label = (memory_label > 0.0).to(torch::kFloat32);
            memory_label_tensor_list[memory_pair.first] = memory_label;
        mask_vector[memory_pair.first] = 1.0;

    }

    for (auto i = 0; i < mask_vector.size(); i ++)
    {
        if (mask_vector[i] == 0.0) {
            memory_frame_tensor_list[i] = torch::zeros({1, 3, 256, 256},torch::kFloat32).to(device);
            memory_label_tensor_list[i] = torch::zeros({1, 1, 256, 256}, torch::kFloat32).to(device);
        }
    }

    _memory_tensors->memory_frame_tensor = torch::stack(memory_frame_tensor_list,1);
    _memory_tensors->memory_label_tensor = torch::stack(memory_label_tensor_list,1);
    _memory_tensors->mask_tensor = torch::tensor(mask_vector).unsqueeze(0).to(device);

}

std::vector<Point2D<float>> SCM::process_frame(std::vector<uint8_t>& image, int height, int width) {

    device = dl::get_device();

    if (!module) {
        module = dl::load_torchscript_model("/home/wanglab/Desktop/efficientvit_pytorch_cuda3.pt", device);
    }

    module->to(device);

    if (_memory.empty())
    {
        std::cout << "Currently no frames in memory. Please select some" << std::endl;
        return std::vector<Point2D<float>>();
    }

    auto image_tensor = convert_image_vec_to_tensor(image, height, width);

    //data_input = data_input.repeat({100, 1, 1, 1});

    torch::NoGradGuard no_grad;

    //torch::jit::setGraphExecutorOptimize(false);
    auto output = module->forward(
                            {image_tensor,
                             _memory_tensors->memory_frame_tensor.to(device),
                             _memory_tensors->memory_label_tensor.to(device),
                             _memory_tensors->mask_tensor.to(device)}).toTensor();

    output = output.mul(255).clamp(0,255).to(torch::kU8).detach().to(torch::kCPU);
    std::vector<uint8_t> vec(output.data_ptr<uint8_t>(), output.data_ptr<uint8_t>() + output.numel());

    std::transform(vec.begin(), vec.end(), vec.begin(), [](uint8_t pixel) {
        return pixel > 1 ? 1 : 0;
    });

    auto t1 = std::chrono::high_resolution_clock::now();

    auto output_vec = fast_skeletonize(vec, 256, 256);

    auto t2 = std::chrono::high_resolution_clock::now();

    auto output_line = order_line(output_vec, 256, 256, {_x,_y});

    auto t3 = std::chrono::high_resolution_clock::now();

    std::chrono::duration<double> elapsed = t2 - t1;
    //std::cout << "Time for Skeletonization: " << elapsed.count() << std::endl;

    elapsed = t3 - t1;
    //std::cout << "Time for line ordering: " << elapsed.count() << std::endl;

    return output_line;
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

    if (key_index > 0) {
        auto temp = std::move(_memory[0]);
        _memory[0] = memory_frame_pair{memory_frame, memory_label};
        _memory[key_index] = temp;
    } else {
        _memory[key_index] = memory_frame_pair{memory_frame, memory_label};
    }

    _create_memory_tensors();

    std::cout << "memory frame added at " << key_index << std::endl;
}


}
