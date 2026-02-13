#include "ModelExecution.hpp"

#include "backends/AOTInductorBackend.hpp"
#include "backends/TorchScriptBackend.hpp"

#ifdef DL_HAS_EXECUTORCH
#include "backends/ExecuTorchBackend.hpp"
#endif

#include <iostream>
#include <stdexcept>
#include <utility>

namespace dl {

// ---------------------------------------------------------------------------
// Construction / destruction / move
// ---------------------------------------------------------------------------
ModelExecution::ModelExecution(BackendType backend)
    : _requested_backend(backend)
{
}

ModelExecution::~ModelExecution() = default;
ModelExecution::ModelExecution(ModelExecution &&) noexcept = default;
ModelExecution & ModelExecution::operator=(ModelExecution &&) noexcept = default;

// ---------------------------------------------------------------------------
// createBackend (static factory)
// ---------------------------------------------------------------------------
std::unique_ptr<InferenceBackend> ModelExecution::createBackend(BackendType type)
{
    switch (type) {
        case BackendType::TorchScript:
            return std::make_unique<TorchScriptBackend>();

        case BackendType::AOTInductor:
            return std::make_unique<AOTInductorBackend>();

        case BackendType::ExecuTorch:
#ifdef DL_HAS_EXECUTORCH
            return std::make_unique<ExecuTorchBackend>();
#else
            throw std::runtime_error(
                "[ModelExecution] ExecuTorch backend is not available. "
                "Rebuild with -DENABLE_EXECUTORCH=ON to enable .pte support.");
#endif

        case BackendType::Auto:
            throw std::runtime_error(
                "[ModelExecution] Cannot create backend with BackendType::Auto. "
                "Call load() which resolves Auto from file extension.");
    }

    throw std::runtime_error("[ModelExecution] Unknown BackendType");
}

// ---------------------------------------------------------------------------
// load
// ---------------------------------------------------------------------------
bool ModelExecution::load(std::filesystem::path const & path)
{
    // Determine which backend to use
    BackendType effective = _requested_backend;
    if (effective == BackendType::Auto) {
        effective = backendTypeFromExtension(path);
        if (effective == BackendType::Auto) {
            std::cerr << "[ModelExecution] Cannot determine backend from extension: "
                      << path.extension() << "\n"
                      << "  Supported extensions: .pt (TorchScript), .pt2 (AOTInductor), .pte (ExecuTorch)\n";
            return false;
        }
    }

    try {
        auto backend = createBackend(effective);
        if (!backend->load(path)) {
            return false;
        }
        _backend = std::move(backend);
        return true;

    } catch (std::exception const & e) {
        std::cerr << "[ModelExecution] Failed to create/load backend: "
                  << e.what() << "\n";
        return false;
    }
}

// ---------------------------------------------------------------------------
// isLoaded / loadedPath / activeBackend / activeBackendName
// ---------------------------------------------------------------------------
bool ModelExecution::isLoaded() const
{
    return _backend && _backend->isLoaded();
}

std::filesystem::path ModelExecution::loadedPath() const
{
    return _backend ? _backend->loadedPath() : std::filesystem::path{};
}

BackendType ModelExecution::activeBackend() const
{
    return _backend ? _backend->type() : _requested_backend;
}

std::string ModelExecution::activeBackendName() const
{
    return _backend ? _backend->name() : backendTypeToString(_requested_backend);
}

// ---------------------------------------------------------------------------
// execute (forward method, ordered inputs)
// ---------------------------------------------------------------------------
std::vector<torch::Tensor>
ModelExecution::execute(std::vector<torch::Tensor> const & inputs)
{
    if (!isLoaded()) {
        throw std::runtime_error("[ModelExecution] No model loaded");
    }
    return _backend->execute(inputs);
}

// ---------------------------------------------------------------------------
// execute (named method, ordered inputs)
// ---------------------------------------------------------------------------
std::vector<torch::Tensor>
ModelExecution::execute(std::string const & method_name,
                        std::vector<torch::Tensor> const & inputs)
{
    if (!isLoaded()) {
        throw std::runtime_error("[ModelExecution] No model loaded");
    }
    return _backend->execute(method_name, inputs);
}

// ---------------------------------------------------------------------------
// executeNamed
// ---------------------------------------------------------------------------
std::vector<torch::Tensor>
ModelExecution::executeNamed(
    std::unordered_map<std::string, torch::Tensor> const & named_inputs,
    std::vector<std::string> const & input_order)
{
    std::vector<torch::Tensor> ordered;
    ordered.reserve(input_order.size());
    for (auto const & name : input_order) {
        auto it = named_inputs.find(name);
        if (it == named_inputs.end()) {
            throw std::runtime_error(
                "[ModelExecution] Missing input tensor for slot: " + name);
        }
        ordered.push_back(it->second);
    }
    return execute(ordered);
}

} // namespace dl
