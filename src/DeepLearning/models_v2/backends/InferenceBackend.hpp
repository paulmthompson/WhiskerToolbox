/**
 * @file InferenceBackend.hpp
 * @brief Abstract inference backend interface and backend type utilities.
 */

#ifndef NEURALYZER_INFERENCE_BACKEND_HPP
#define NEURALYZER_INFERENCE_BACKEND_HPP

#include "BackendType.hpp"// BackendType

#include <ATen/core/Tensor.h>// at::Tensor

#include <cctype>
#include <filesystem>
#include <string>
#include <vector>

namespace dl {

/**
 * @brief Detect backend type from a file extension.
 *
 * @return The detected BackendType, or BackendType::Auto if extension is not
 *         recognized (caller should treat as an error or fall back).
 */
[[nodiscard]] inline BackendType backendTypeFromExtension(std::filesystem::path const & path) {
    auto ext = path.extension().string();
    for (auto & c: ext) {
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

/**
 * @brief Abstract interface for model inference backends.
 *
 * Each backend knows how to load a model file and execute inference,
 * producing `at::Tensor` outputs from `at::Tensor` inputs.
 * The `at::Tensor` type is the common currency across all backends.
 */
class InferenceBackend {
public:
    virtual ~InferenceBackend() = default;

    /**
     * @brief Human-readable backend name (e.g. "TorchScript", "AOTInductor").
     */
    [[nodiscard]] virtual std::string name() const = 0;

    /**
     * @brief The BackendType enum value for this backend.
     */
    [[nodiscard]] virtual BackendType type() const = 0;

    /**
     * @brief File extension this backend handles (e.g. ".pt", ".pt2", ".pte").
     */
    [[nodiscard]] virtual std::string fileExtension() const = 0;

    /**
     * @brief Load model weights/program from a file.
     *
     * @param path Path to the model file.
     * @return true if loading succeeded.
     */
    virtual bool load(std::filesystem::path const & path) = 0;

    /**
     * @brief Whether a model is loaded and ready for execution.
     */
    [[nodiscard]] virtual bool isLoaded() const = 0;

    /**
     * @brief Get the path of the currently loaded model.
     */
    [[nodiscard]] virtual std::filesystem::path loadedPath() const = 0;

    /**
     * @brief Execute the default method (typically "forward") with ordered inputs.
     *
     * All tensors are `at::Tensor` (libtorch) — the common currency
     * across all backends.
     *
     * @param inputs Ordered vector of input tensors.
     * @return Ordered vector of output tensors.
     * @throws std::runtime_error if the model is not loaded or execution fails.
     */
    [[nodiscard]] virtual std::vector<at::Tensor>
    execute(std::vector<at::Tensor> const & inputs) = 0;

    /**
     * @brief Execute a named method with ordered inputs.
     *
     * Not all backends support multiple methods. Backends that don't should
     * ignore the method_name and execute the default model.
     *
     * @param method_name Name of the method to execute (e.g. "forward", "encode").
     * @param inputs Ordered vector of input tensors.
     * @return Ordered vector of output tensors.
     * @throws std::runtime_error if the model is not loaded or execution fails.
     */
    [[nodiscard]] virtual std::vector<at::Tensor>
    execute(std::string const & method_name,
            std::vector<at::Tensor> const & inputs) = 0;

    // Non-copyable (subclasses own heavyweight resources)
    InferenceBackend(InferenceBackend const &) = delete;
    InferenceBackend & operator=(InferenceBackend const &) = delete;

protected:
    InferenceBackend() = default;
    InferenceBackend(InferenceBackend &&) = default;
    InferenceBackend & operator=(InferenceBackend &&) = default;
};

}// namespace dl

#endif// NEURALYZER_INFERENCE_BACKEND_HPP
