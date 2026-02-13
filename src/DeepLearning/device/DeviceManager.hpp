#ifndef WHISKERTOOLBOX_DEVICE_MANAGER_HPP
#define WHISKERTOOLBOX_DEVICE_MANAGER_HPP

#include <torch/torch.h>

namespace dl {

/// Centralized, lazily-initialized device context for all deep-learning
/// model wrappers and inference code.
///
/// Usage:
/// @code
///   auto & dm = dl::DeviceManager::instance();
///   auto tensor_on_device = dm.toDevice(cpu_tensor);
///   auto dev = dm.device();  // torch::kCUDA or torch::kCPU
/// @endcode
///
/// At first access, auto-detects CUDA availability and selects GPU if present.
/// Call `setDevice()` to override (e.g. for testing or user preference).
class DeviceManager {
public:
    /// Get the singleton instance (thread-safe via Meyers' singleton).
    static DeviceManager & instance();

    /// The active device. Determined once at first access:
    /// CUDA if available, else CPU.
    [[nodiscard]] torch::Device device() const;

    /// Force a specific device (useful for testing or user preference).
    void setDevice(torch::Device dev);

    /// Whether CUDA is available on this system.
    [[nodiscard]] static bool cudaAvailable();

    /// Move a tensor to the active device. Returns the tensor unchanged
    /// if it is already on the correct device.
    [[nodiscard]] torch::Tensor toDevice(torch::Tensor tensor) const;

    // Non-copyable, non-movable
    DeviceManager(DeviceManager const &) = delete;
    DeviceManager & operator=(DeviceManager const &) = delete;
    DeviceManager(DeviceManager &&) = delete;
    DeviceManager & operator=(DeviceManager &&) = delete;

private:
    DeviceManager();
    torch::Device _device;
};

} // namespace dl

#endif // WHISKERTOOLBOX_DEVICE_MANAGER_HPP
