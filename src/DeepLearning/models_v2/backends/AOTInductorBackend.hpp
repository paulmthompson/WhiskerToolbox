#ifndef WHISKERTOOLBOX_AOT_INDUCTOR_BACKEND_HPP
#define WHISKERTOOLBOX_AOT_INDUCTOR_BACKEND_HPP

#include "InferenceBackend.hpp"

#include <filesystem>
#include <memory>
#include <string>
#include <vector>

namespace dl {

/// AOT Inductor inference backend — loads `.pt2` packages via
/// `torch::inductor::AOTIModelPackageLoader`.
///
/// This is the **recommended backend** for new model deployment. AOT Inductor
/// produces ahead-of-time compiled native code (CPU or CUDA kernels) from
/// `torch.export()` + `aoti_compile_and_package()` in Python.
///
/// Key characteristics:
/// - Best inference performance — native compiled kernels with autotuning
/// - Operator fusion and optimization done at compile time
/// - Supports dynamic shapes (specified at export via `torch.export.Dim()`)
/// - Supports CUDA and CPU execution
/// - Headers are part of the bundled libtorch 2.9.0 distribution
///
/// @note AOT Inductor does not support multiple named methods per package.
///       The `method_name` parameter in `execute()` is ignored; the compiled
///       model's single entry point is always invoked.
class AOTInductorBackend : public InferenceBackend {
public:
    AOTInductorBackend();
    ~AOTInductorBackend() override;

    // Movable
    AOTInductorBackend(AOTInductorBackend &&) noexcept;
    AOTInductorBackend & operator=(AOTInductorBackend &&) noexcept;

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

#endif // WHISKERTOOLBOX_AOT_INDUCTOR_BACKEND_HPP
