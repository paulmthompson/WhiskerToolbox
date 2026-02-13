#ifndef WHISKERTOOLBOX_MODEL_EXECUTION_HPP
#define WHISKERTOOLBOX_MODEL_EXECUTION_HPP

#include "backends/InferenceBackend.hpp"

#include <torch/torch.h>

#include <filesystem>
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

namespace dl {

/// Multi-backend model execution layer.
///
/// `ModelExecution` dispatches inference to one of several backends
/// (TorchScript, AOT Inductor, ExecuTorch) via the `InferenceBackend`
/// strategy pattern. The backend is selected either explicitly via
/// `BackendType` or auto-detected from the model file extension:
///
///   - `.pt`  → TorchScript   (legacy, deprecated but functional)
///   - `.pt2` → AOT Inductor  (recommended for new models)
///   - `.pte` → ExecuTorch    (optional, requires ENABLE_EXECUTORCH)
///
/// The public API is identical to the previous ExecuTorch-only implementation,
/// so downstream code (`NeuroSAMModel`, `RuntimeModel`, `SlotAssembler`)
/// requires no changes.
///
/// Usage:
/// @code
///   // Auto-detect from extension:
///   dl::ModelExecution exec;
///   exec.load("/path/to/model.pt2");     // → AOTInductorBackend
///
///   // Explicit backend:
///   dl::ModelExecution exec2(dl::BackendType::TorchScript);
///   exec2.load("/path/to/model.pt");     // → TorchScriptBackend
///
///   if (exec.isLoaded()) {
///       auto outputs = exec.execute({input_tensor_1, input_tensor_2});
///   }
/// @endcode
class ModelExecution {
public:
    /// Create with auto-detection (default) or explicit backend selection.
    ///
    /// @param backend  BackendType::Auto selects backend from file extension
    ///                 at load() time. Other values force a specific backend.
    explicit ModelExecution(BackendType backend = BackendType::Auto);
    ~ModelExecution();

    // Non-copyable, movable
    ModelExecution(ModelExecution const &) = delete;
    ModelExecution & operator=(ModelExecution const &) = delete;
    ModelExecution(ModelExecution &&) noexcept;
    ModelExecution & operator=(ModelExecution &&) noexcept;

    /// Load a model file. If BackendType is Auto, selects backend from extension:
    ///   .pt  → TorchScript
    ///   .pt2 → AOTInductor
    ///   .pte → ExecuTorch (requires ENABLE_EXECUTORCH build)
    ///
    /// @param path  Path to the model file.
    /// @return true if loading succeeded.
    bool load(std::filesystem::path const & path);

    /// Whether a model is loaded and ready for execution.
    [[nodiscard]] bool isLoaded() const;

    /// Get the path of the currently loaded model.
    [[nodiscard]] std::filesystem::path loadedPath() const;

    /// Get the active backend type (Auto if no model is loaded yet).
    [[nodiscard]] BackendType activeBackend() const;

    /// Get the active backend's human-readable name.
    [[nodiscard]] std::string activeBackendName() const;

    /// Execute the default "forward" method with ordered input tensors.
    ///
    /// @param inputs  Ordered vector of input tensors.
    /// @return        Ordered vector of output tensors.
    /// @throws std::runtime_error if the model is not loaded or execution fails.
    [[nodiscard]] std::vector<torch::Tensor>
    execute(std::vector<torch::Tensor> const & inputs);

    /// Execute a named method with ordered input tensors.
    ///
    /// @param method_name  Name of the method to execute (e.g. "forward", "encode").
    /// @param inputs       Ordered vector of input tensors.
    /// @return             Ordered vector of output tensors.
    /// @throws std::runtime_error if the model is not loaded or execution fails.
    [[nodiscard]] std::vector<torch::Tensor>
    execute(std::string const & method_name, std::vector<torch::Tensor> const & inputs);

    /// Execute the "forward" method with named input tensors.
    ///
    /// Reorders named inputs according to `input_order`, then delegates
    /// to `execute()`.
    ///
    /// @param named_inputs  Map of slot_name → Tensor.
    /// @param input_order   Ordered slot names matching the model's forward() signature.
    /// @return              Ordered vector of output tensors.
    [[nodiscard]] std::vector<torch::Tensor>
    executeNamed(std::unordered_map<std::string, torch::Tensor> const & named_inputs,
                 std::vector<std::string> const & input_order);

private:
    /// Create the appropriate backend for the given type.
    /// @throws std::runtime_error if the backend is not available (e.g. ExecuTorch
    ///         without ENABLE_EXECUTORCH).
    static std::unique_ptr<InferenceBackend> createBackend(BackendType type);

    std::unique_ptr<InferenceBackend> _backend;
    BackendType _requested_backend;
};

} // namespace dl

#endif // WHISKERTOOLBOX_MODEL_EXECUTION_HPP
