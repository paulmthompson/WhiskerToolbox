#ifndef WHISKERTOOLBOX_MODEL_EXECUTION_HPP
#define WHISKERTOOLBOX_MODEL_EXECUTION_HPP

#include <torch/torch.h>

#include <filesystem>
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

namespace executorch::extension::module {
class Module;
} // namespace executorch::extension::module

namespace dl {

/// Wraps the ExecuTorch C++ runtime to load `.pte` programs and run inference.
///
/// ModelExecution bridges between the libtorch `torch::Tensor` world (used by
/// ChannelEncoders/Decoders and the rest of the pipeline) and the ExecuTorch
/// `EValue`-based execution API.
///
/// Usage:
/// @code
///   dl::ModelExecution exec;
///   exec.load("/path/to/model.pte");
///   if (exec.isLoaded()) {
///       auto outputs = exec.execute({input_tensor_1, input_tensor_2});
///   }
/// @endcode
class ModelExecution {
public:
    ModelExecution();
    ~ModelExecution();

    // Non-copyable, movable
    ModelExecution(ModelExecution const &) = delete;
    ModelExecution & operator=(ModelExecution const &) = delete;
    ModelExecution(ModelExecution &&) noexcept;
    ModelExecution & operator=(ModelExecution &&) noexcept;

    /// Load an ExecuTorch program (.pte file).
    ///
    /// @param pte_path  Path to the .pte program file.
    /// @return true if loading succeeded.
    bool load(std::filesystem::path const & pte_path);

    /// Whether a program is loaded and ready for execution.
    [[nodiscard]] bool isLoaded() const;

    /// Get the path of the currently loaded program.
    [[nodiscard]] std::filesystem::path loadedPath() const;

    /// Execute the "forward" method with ordered input tensors.
    ///
    /// Input tensors are provided in the order expected by the model's forward
    /// method. Each `torch::Tensor` is converted to an ExecuTorch EValue for
    /// execution, and output EValues are converted back to `torch::Tensor`.
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
    /// This is a convenience wrapper that reorders named inputs according to the
    /// model's expected input order (slot names). Requires that `input_order` has
    /// been set via `setInputOrder()`.
    ///
    /// @param named_inputs  Map of slot_name → Tensor.
    /// @param input_order   Ordered slot names matching the model's forward() signature.
    /// @return              Ordered vector of output tensors.
    [[nodiscard]] std::vector<torch::Tensor>
    executeNamed(std::unordered_map<std::string, torch::Tensor> const & named_inputs,
                 std::vector<std::string> const & input_order);

private:
    struct Impl;
    std::unique_ptr<Impl> _impl;
};

} // namespace dl

#endif // WHISKERTOOLBOX_MODEL_EXECUTION_HPP
