/**
 * @file DeviceManager.cpp
 * @brief Implementation of the centralized deep-learning device manager.
 */

#include "DeviceManager.hpp"

#include <torch/cuda.h> // torch::cuda::is_available
#include <c10/core/DeviceType.h> // at::Device

#include <iostream>

namespace dl {

DeviceManager::DeviceManager()
    : _device(at::kCPU) {
    if (torch::cuda::is_available()) {
        _device = at::Device(at::kCUDA);
        std::cout << "[DeviceManager] CUDA is available — using GPU.\n";
    } else {
        std::cout << "[DeviceManager] No GPU found — using CPU.\n";
    }
}

DeviceManager & DeviceManager::instance() {
    static DeviceManager inst;
    return inst;
}

at::Device DeviceManager::device() const {
    return _device;
}

void DeviceManager::setDevice(at::Device dev) {
    _device = dev;
}

bool DeviceManager::cudaAvailable() {
    return torch::cuda::is_available();
}

at::Tensor DeviceManager::toDevice(at::Tensor tensor) const {
    if (tensor.device() == _device) {
        return tensor;
    }
    return tensor.to(_device);
}

}// namespace dl
