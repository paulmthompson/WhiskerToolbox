#include "TorchScriptBackend.hpp"

#include "device/DeviceManager.hpp"

#include <torch/script.h>

#include <iostream>
#include <sstream>
#include <stdexcept>
#include <utility>

namespace dl {

// ---------------------------------------------------------------------------
// Helper: extract tensors from a JIT IValue (handles single tensor or tuple)
// ---------------------------------------------------------------------------
namespace {

std::vector<torch::Tensor> iValueToTensors(torch::jit::IValue const & value)
{
    std::vector<torch::Tensor> result;

    if (value.isTensor()) {
        result.push_back(value.toTensor());
    } else if (value.isTuple()) {
        auto const & elements = value.toTuple()->elements();
        result.reserve(elements.size());
        for (auto const & elem : elements) {
            if (elem.isTensor()) {
                result.push_back(elem.toTensor());
            }
        }
    } else if (value.isTensorList()) {
        auto const list = value.toTensorList();
        result.reserve(list.size());
        for (auto const & t : list) {
            result.push_back(t);
        }
    }

    return result;
}

} // anonymous namespace

// ---------------------------------------------------------------------------
// PIMPL
// ---------------------------------------------------------------------------
struct TorchScriptBackend::Impl {
    torch::jit::Module module;
    std::filesystem::path loaded_path;
    bool is_loaded = false;
};

// ---------------------------------------------------------------------------
// Construction / destruction / move
// ---------------------------------------------------------------------------
TorchScriptBackend::TorchScriptBackend()
    : _impl(std::make_unique<Impl>())
{
}

TorchScriptBackend::~TorchScriptBackend() = default;
TorchScriptBackend::TorchScriptBackend(TorchScriptBackend &&) noexcept = default;
TorchScriptBackend & TorchScriptBackend::operator=(TorchScriptBackend &&) noexcept = default;

// ---------------------------------------------------------------------------
// Metadata
// ---------------------------------------------------------------------------
std::string TorchScriptBackend::name() const { return "TorchScript"; }
BackendType TorchScriptBackend::type() const { return BackendType::TorchScript; }
std::string TorchScriptBackend::fileExtension() const { return ".pt"; }

// ---------------------------------------------------------------------------
// load
// ---------------------------------------------------------------------------
bool TorchScriptBackend::load(std::filesystem::path const & path)
{
    try {
        auto & dm = DeviceManager::instance();
        auto module = torch::jit::load(path.string(), dm.device());
        module.to(dm.device());
        module.eval();

        _impl->module = std::move(module);
        _impl->loaded_path = path;
        _impl->is_loaded = true;
        return true;

    } catch (c10::Error const & e) {
        std::cerr << "[TorchScriptBackend] c10 error loading " << path
                  << ": " << e.what() << "\n";
        _impl->is_loaded = false;
        return false;
    } catch (std::exception const & e) {
        std::cerr << "[TorchScriptBackend] Exception loading " << path
                  << ": " << e.what() << "\n";
        _impl->is_loaded = false;
        return false;
    }
}

// ---------------------------------------------------------------------------
// isLoaded / loadedPath
// ---------------------------------------------------------------------------
bool TorchScriptBackend::isLoaded() const
{
    return _impl->is_loaded;
}

std::filesystem::path TorchScriptBackend::loadedPath() const
{
    return _impl->loaded_path;
}

// ---------------------------------------------------------------------------
// execute (default "forward" method)
// ---------------------------------------------------------------------------
std::vector<torch::Tensor>
TorchScriptBackend::execute(std::vector<torch::Tensor> const & inputs)
{
    return execute("forward", inputs);
}

// ---------------------------------------------------------------------------
// execute (named method)
// ---------------------------------------------------------------------------
std::vector<torch::Tensor>
TorchScriptBackend::execute(std::string const & method_name,
                            std::vector<torch::Tensor> const & inputs)
{
    if (!isLoaded()) {
        throw std::runtime_error("[TorchScriptBackend] No model loaded");
    }

    auto & dm = DeviceManager::instance();

    // Convert inputs to JIT IValues on the correct device
    std::vector<torch::jit::IValue> jit_inputs;
    jit_inputs.reserve(inputs.size());
    for (auto const & t : inputs) {
        jit_inputs.emplace_back(dm.toDevice(t));
    }

    try {
        torch::jit::IValue output;
        if (method_name == "forward") {
            output = _impl->module.forward(jit_inputs);
        } else {
            output = _impl->module.get_method(method_name)(jit_inputs);
        }

        return iValueToTensors(output);

    } catch (c10::Error const & e) {
        std::ostringstream oss;
        oss << "[TorchScriptBackend] Execution of method '" << method_name
            << "' failed: " << e.what() << "\n"
            << "  Number of inputs: " << inputs.size() << "\n";
        for (size_t i = 0; i < inputs.size(); ++i) {
            oss << "    Input[" << i << "]: shape=" << inputs[i].sizes()
                << ", dtype=" << inputs[i].dtype()
                << ", device=" << inputs[i].device() << "\n";
        }
        throw std::runtime_error(oss.str());
    }
}

} // namespace dl
