#ifndef TORCH_HELPERS_HPP
#define TORCH_HELPERS_HPP

#include "CoreGeometry/ImageSize.hpp"

#include <ATen/core/Tensor.h>// at::Tensor
#include <ATen/Functions.h> // at::empty
#include <torch/cuda.h>  // torch::cuda::is_available
#include <c10/core/DeviceType.h> // at::kCPU, at::kCUDA
#include <c10/core/Device.h> // at::Device
#include <ATen/Parallel.h> // at::init_num_threads, at::get_num_threads
#include <torch/script.h>

#include <memory>
#include <string>

namespace dl {

static at::Device device(at::kCPU);

inline at::Device get_device(){
    auto device = at::Device(at::kCPU);
    if (torch::cuda::is_available()) {
        std::cout << "CUDA is available! Using the GPU." << std::endl;
        device = at::Device(at::kCUDA);
    } else {
        std::cout << "No GPU found" << std::endl;
    }
    return device;
}

inline std::shared_ptr<torch::jit::Module> load_torchscript_model(std::string const & model_file_path, at::Device device)
{
    try {
        // Deserialize the ScriptModule from a file using torch::jit::load().
        auto module = std::make_shared<torch::jit::Module>(torch::jit::load(model_file_path));
        module->to(device);
        module->to(at::kFloat);
        module->eval();

        //https://discuss.pytorch.org/t/pytorch-script-model-in-c-is-slow-on-prediting-the-second-image/127232/4
        //torch::jit::FusionStrategy static0 = { {torch::jit::FusionBehavior::STATIC, 0} };
        //torch::jit::setFusionStrategy(static0);
        //torch::jit::setGraphExecutorOptimize(false);

        std::cout << "Loaded model" << std::endl;

        at::init_num_threads();

        std::cout << "Number of threads is " << at::get_num_threads() << std::endl;

        return module;
    }
    catch (const c10::Error& e) {
        std::cerr << "error loading the model\n";
        return std::shared_ptr<torch::jit::Module>{nullptr};
    }
}

inline at::Tensor create_tensor_from_gray8(std::vector<uint8_t> const & image, ImageSize const image_size)
{
    auto tensor = at::empty(
        { image_size.height, image_size.width, 1},
        at::TensorOptions()
            .dtype(at::kByte)
            .device(at::kCPU));

    std::memcpy(tensor.data_ptr(), image.data(), tensor.numel() * sizeof(at::kByte));
    tensor = tensor.permute({2,0,1});
    tensor = tensor.unsqueeze(0);
    return tensor;
}





}
#endif // TORCH_HELPERS_HPP
