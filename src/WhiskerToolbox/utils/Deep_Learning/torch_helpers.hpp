#ifndef TORCH_HELPERS_HPP
#define TORCH_HELPERS_HPP


#include <torch/torch.h>
#include <torch/script.h>

#include <memory>
#include <string>

namespace dl {
torch::Device get_device(){
    auto device = torch::Device(torch::kCPU);
    if (torch::cuda::is_available()) {
        std::cout << "CUDA is available! Using the GPU." << std::endl;
        device = torch::Device(torch::kCUDA);
    } else {
        std::cout << "No GPU found" << std::endl;
    }
    return device;
}

std::shared_ptr<torch::jit::Module> load_torchscript_model(std::string model_file_path, torch::Device device)
{
    try {
        // Deserialize the ScriptModule from a file using torch::jit::load().
        auto module = std::make_shared<torch::jit::Module>(torch::jit::load(model_file_path));
        module->to(device);
        module->to(torch::kFloat32);
        module->eval();

        //https://discuss.pytorch.org/t/pytorch-script-model-in-c-is-slow-on-prediting-the-second-image/127232/4
        //torch::jit::FusionStrategy static0 = { {torch::jit::FusionBehavior::STATIC, 0} };
        //torch::jit::setFusionStrategy(static0);
        //torch::jit::setGraphExecutorOptimize(false);

        std::cout << "Loaded model" << std::endl;

        torch::init_num_threads();

        std::cout << "Number of threads is " << torch::get_num_threads() << std::endl;

        return module;
    }
    catch (const c10::Error& e) {
        std::cerr << "error loading the model\n";
        return std::shared_ptr<torch::jit::Module>(nullptr);
    }

}
}
#endif // TORCH_HELPERS_HPP
