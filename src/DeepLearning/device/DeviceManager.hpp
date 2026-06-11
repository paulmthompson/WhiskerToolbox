/**
 * @file DeviceManager.hpp
 * @brief Centralized device context for deep-learning inference.
 */

#ifndef WHISKERTOOLBOX_DEVICE_MANAGER_HPP
#define WHISKERTOOLBOX_DEVICE_MANAGER_HPP

#include <ATen/core/Tensor.h> // at::Tensor
#include <c10/core/Device.h> // at::Device

namespace dl {

/**
 * @brief Centralized, lazily-initialized device context for all deep-learning
 *        model wrappers and inference code.
 *
 * Usage:
 * @code
 *   auto & dm = dl::DeviceManager::instance();
 *   auto tensor_on_device = dm.toDevice(cpu_tensor);
 *   auto dev = dm.device();  // torch::kCUDA or torch::kCPU
 * @endcode
 *
 * At first access, auto-detects CUDA availability and selects GPU if present.
 * Call `setDevice()` to override (e.g. for testing or user preference).
 */
class DeviceManager {
public:
    /**
     * @brief Get the singleton instance (thread-safe via Meyers' singleton).
     */
    static DeviceManager & instance();

    /**
     * @brief The active device. Determined once at first access.
     *
     * CUDA if available, else CPU.
     */
    [[nodiscard]] at::Device device() const;

    /**
     * @brief Force a specific device (useful for testing or user preference).
     */
    void setDevice(at::Device dev);

    /**
     * @brief Whether CUDA is available on this system.
     */
    [[nodiscard]] static bool cudaAvailable();

    /**
     * @brief Move a tensor to the active device.
     *
     * Returns the tensor unchanged if it is already on the correct device.
     */
    [[nodiscard]] at::Tensor toDevice(at::Tensor tensor) const;

    // Non-copyable, non-movable
    DeviceManager(DeviceManager const &) = delete;
    DeviceManager & operator=(DeviceManager const &) = delete;
    DeviceManager(DeviceManager &&) = delete;
    DeviceManager & operator=(DeviceManager &&) = delete;

private:
    DeviceManager();
    at::Device _device;
};

}// namespace dl

#endif// WHISKERTOOLBOX_DEVICE_MANAGER_HPP
