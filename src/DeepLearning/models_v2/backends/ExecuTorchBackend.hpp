#ifndef WHISKERTOOLBOX_EXECUTORCH_BACKEND_HPP
#define WHISKERTOOLBOX_EXECUTORCH_BACKEND_HPP

#include "InferenceBackend.hpp"

#include <filesystem>
#include <memory>
#include <string>
#include <vector>

namespace dl {

/// ExecuTorch inference backend — loads `.pte` programs.
///
/// This backend wraps the ExecuTorch C++ runtime to load and execute
/// ExecuTorch programs. It bridges between libtorch `torch::Tensor`
/// and ExecuTorch's `EValue` execution API.
///
/// This backend is **optional** and only available when the project is
/// built with `ENABLE_EXECUTORCH=ON`. It is designed for edge/mobile
/// deployment scenarios.
///
/// @note ExecuTorch operates on CPU only in the default configuration.
///       Input tensors are converted to contiguous CPU tensors before
///       execution.
class ExecuTorchBackend : public InferenceBackend {
public:
    ExecuTorchBackend();
    ~ExecuTorchBackend() override;

    // Movable
    ExecuTorchBackend(ExecuTorchBackend &&) noexcept;
    ExecuTorchBackend & operator=(ExecuTorchBackend &&) noexcept;

    [[nodiscard]] std::string name() const override;
    [[nodiscard]] BackendType type() const override;
    [[nodiscard]] std::string fileExtension() const override;

    bool load(std::filesystem::path const & path) override;
    [[nodiscard]] bool isLoaded() const override;
    [[nodiscard]] std::filesystem::path loadedPath() const override;

    [[nodiscard]] std::vector<torch::Tensor>
    execute(std::vector<torch::Tensor> const & inputs) override;

    [[nodiscard]] std::vector<torch::Tensor>
    execute(std::string const & method_name,
            std::vector<torch::Tensor> const & inputs) override;

private:
    struct Impl;
    std::unique_ptr<Impl> _impl;
};

} // namespace dl

#endif // WHISKERTOOLBOX_EXECUTORCH_BACKEND_HPP
