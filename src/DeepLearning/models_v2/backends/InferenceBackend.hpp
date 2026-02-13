#ifndef WHISKERTOOLBOX_INFERENCE_BACKEND_HPP
#define WHISKERTOOLBOX_INFERENCE_BACKEND_HPP

#include <torch/torch.h>

#include <filesystem>
#include <string>
#include <vector>

namespace dl {

/// Serialization format / inference backend for model execution.
enum class BackendType {
    TorchScript,  ///< .pt files via torch::jit::load() (deprecated but functional)
    AOTInductor,  ///< .pt2 files via AOTIModelPackageLoader (recommended)
    ExecuTorch,   ///< .pte files via ExecuTorch runtime (optional, edge deployment)
    Auto          ///< Auto-detect from file extension
};

/// Convert a BackendType to a human-readable string.
[[nodiscard]] inline std::string backendTypeToString(BackendType type)
{
    switch (type) {
        case BackendType::TorchScript: return "TorchScript";
        case BackendType::AOTInductor: return "AOTInductor";
        case BackendType::ExecuTorch:  return "ExecuTorch";
        case BackendType::Auto:        return "Auto";
    }
    return "Unknown";
}

/// Parse a BackendType from a string (case-insensitive).
/// Returns BackendType::Auto if the string is not recognized.
[[nodiscard]] inline BackendType backendTypeFromString(std::string const & str)
{
    // Simple case-insensitive compare via lowering
    auto lower = str;
    for (auto & c : lower) {
        c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    }

    if (lower == "torchscript" || lower == "torch_script" || lower == "jit") {
        return BackendType::TorchScript;
    }
    if (lower == "aotinductor" || lower == "aot_inductor" || lower == "inductor" || lower == "aoti") {
        return BackendType::AOTInductor;
    }
    if (lower == "executorch" || lower == "exec_torch") {
        return BackendType::ExecuTorch;
    }
    return BackendType::Auto;
}

/// Detect backend type from a file extension.
///
/// @return The detected BackendType, or BackendType::Auto if extension is not
///         recognized (caller should treat as an error or fall back).
[[nodiscard]] inline BackendType backendTypeFromExtension(std::filesystem::path const & path)
{
    auto ext = path.extension().string();
    for (auto & c : ext) {
        c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    }

    if (ext == ".pt") {
        return BackendType::TorchScript;
    }
    if (ext == ".pt2") {
        return BackendType::AOTInductor;
    }
    if (ext == ".pte") {
        return BackendType::ExecuTorch;
    }
    return BackendType::Auto;
}

/// Abstract interface for model inference backends.
///
/// Each backend knows how to load a model file and execute inference,
/// producing `torch::Tensor` outputs from `torch::Tensor` inputs.
/// The `torch::Tensor` type is the common currency across all backends.
class InferenceBackend {
public:
    virtual ~InferenceBackend() = default;

    /// Human-readable backend name (e.g. "TorchScript", "AOTInductor").
    [[nodiscard]] virtual std::string name() const = 0;

    /// The BackendType enum value for this backend.
    [[nodiscard]] virtual BackendType type() const = 0;

    /// File extension this backend handles (e.g. ".pt", ".pt2", ".pte").
    [[nodiscard]] virtual std::string fileExtension() const = 0;

    /// Load model weights/program from a file.
    ///
    /// @param path  Path to the model file.
    /// @return true if loading succeeded.
    virtual bool load(std::filesystem::path const & path) = 0;

    /// Whether a model is loaded and ready for execution.
    [[nodiscard]] virtual bool isLoaded() const = 0;

    /// Get the path of the currently loaded model.
    [[nodiscard]] virtual std::filesystem::path loadedPath() const = 0;

    /// Execute the default method (typically "forward") with ordered inputs.
    ///
    /// All tensors are `torch::Tensor` (libtorch) — the common currency
    /// across all backends.
    ///
    /// @param inputs  Ordered vector of input tensors.
    /// @return        Ordered vector of output tensors.
    /// @throws std::runtime_error if the model is not loaded or execution fails.
    [[nodiscard]] virtual std::vector<torch::Tensor>
    execute(std::vector<torch::Tensor> const & inputs) = 0;

    /// Execute a named method with ordered inputs.
    ///
    /// Not all backends support multiple methods. Backends that don't should
    /// ignore the method_name and execute the default model.
    ///
    /// @param method_name  Name of the method to execute (e.g. "forward", "encode").
    /// @param inputs       Ordered vector of input tensors.
    /// @return             Ordered vector of output tensors.
    /// @throws std::runtime_error if the model is not loaded or execution fails.
    [[nodiscard]] virtual std::vector<torch::Tensor>
    execute(std::string const & method_name,
            std::vector<torch::Tensor> const & inputs) = 0;

    // Non-copyable (subclasses own heavyweight resources)
    InferenceBackend(InferenceBackend const &) = delete;
    InferenceBackend & operator=(InferenceBackend const &) = delete;

protected:
    InferenceBackend() = default;
    InferenceBackend(InferenceBackend &&) = default;
    InferenceBackend & operator=(InferenceBackend &&) = default;
};

} // namespace dl

#endif // WHISKERTOOLBOX_INFERENCE_BACKEND_HPP
