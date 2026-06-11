/**
 * @file DeviceManager.cpp
 * @brief Implementation of the centralized deep-learning device manager.
 */

#include "DeviceManager.hpp"

#include <torch/cuda.h> // torch::cuda::is_available
#include <torch/types.h>// torch::Tensor

#include <iostream>

namespace dl {

DeviceManager::DeviceManager()
    : _device(torch::kCPU) {
    if (torch::cuda::is_available()) {
        _device = torch::Device(torch::kCUDA);
        std::cout << "[DeviceManager] CUDA is available — using GPU.\n";
    } else {
        std::cout << "[DeviceManager] No GPU found — using CPU.\n";
    }
}

DeviceManager & DeviceManager::instance() {
    static DeviceManager inst;
    return inst;
}

torch::Device DeviceManager::device() const {
    return _device;
}

void DeviceManager::setDevice(torch::Device dev) {
    _device = dev;
}

bool DeviceManager::cudaAvailable() {
    return torch::cuda::is_available();
}

torch::Tensor DeviceManager::toDevice(torch::Tensor tensor) const {
    if (tensor.device() == _device) {
        return tensor;
    }
    return tensor.to(_device);
}

}// namespace dl
