#ifndef WHISKERTOOLBOX_TORCHSCRIPT_BACKEND_HPP
#define WHISKERTOOLBOX_TORCHSCRIPT_BACKEND_HPP

#include "InferenceBackend.hpp"

#include <torch/script.h>

#include <filesystem>
#include <memory>
#include <string>
#include <vector>

namespace dl {

/// TorchScript inference backend — loads `.pt` files via `torch::jit::load()`.
///
/// This backend uses the TorchScript interpreter built into libtorch to load
/// and execute serialized models produced by `torch.jit.trace()` or
/// `torch.jit.script()` in Python.
///
/// While TorchScript is deprecated by the PyTorch team, it remains fully
/// functional in libtorch 2.9.0 and provides the simplest export path.
/// Legacy models (EfficientSAM, scm) already use this format.
///
/// Key characteristics:
/// - Interpreter-based execution with JIT fusion optimizations
/// - Supports multiple named methods (e.g. "forward", "encode", "decode")
/// - Supports arbitrary dynamic shapes natively
/// - Model runs on the device selected by DeviceManager (CPU or CUDA)
class TorchScriptBackend : public InferenceBackend {
public:
    TorchScriptBackend();
    ~TorchScriptBackend() override;

    // Movable
    TorchScriptBackend(TorchScriptBackend &&) noexcept;
    TorchScriptBackend & operator=(TorchScriptBackend &&) noexcept;

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

#endif // WHISKERTOOLBOX_TORCHSCRIPT_BACKEND_HPP
