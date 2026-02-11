#include "AOTInductorBackend.hpp"

#include "device/DeviceManager.hpp"

#include <torch/csrc/inductor/aoti_package/model_package_loader.h>

#include <iostream>
#include <sstream>
#include <stdexcept>
#include <utility>

namespace dl {

// ---------------------------------------------------------------------------
// PIMPL
// ---------------------------------------------------------------------------
struct AOTInductorBackend::Impl {
    std::unique_ptr<torch::inductor::AOTIModelPackageLoader> loader;
    std::filesystem::path loaded_path;
};

// ---------------------------------------------------------------------------
// Construction / destruction / move
// ---------------------------------------------------------------------------
AOTInductorBackend::AOTInductorBackend()
    : _impl(std::make_unique<Impl>())
{
}

AOTInductorBackend::~AOTInductorBackend() = default;
AOTInductorBackend::AOTInductorBackend(AOTInductorBackend &&) noexcept = default;
AOTInductorBackend & AOTInductorBackend::operator=(AOTInductorBackend &&) noexcept = default;

// ---------------------------------------------------------------------------
// Metadata
// ---------------------------------------------------------------------------
std::string AOTInductorBackend::name() const { return "AOTInductor"; }
BackendType AOTInductorBackend::type() const { return BackendType::AOTInductor; }
std::string AOTInductorBackend::fileExtension() const { return ".pt2"; }

// ---------------------------------------------------------------------------
// load
// ---------------------------------------------------------------------------
bool AOTInductorBackend::load(std::filesystem::path const & path)
{
    try {
        auto & dm = DeviceManager::instance();

        // AOTIModelPackageLoader uses device_index:
        //   -1 = CPU
        //    0 = CUDA:0,  1 = CUDA:1, etc.
        c10::DeviceIndex device_idx = -1;
        if (dm.cudaAvailable()) {
            auto const dev = dm.device();
            if (dev.is_cuda()) {
                device_idx = dev.has_index()
                    ? dev.index()
                    : static_cast<c10::DeviceIndex>(0);
            }
        }

        auto loader = std::make_unique<torch::inductor::AOTIModelPackageLoader>(
            path.string(),
            /*model_name=*/"model",
            /*run_single_threaded=*/false,
            /*num_runners=*/1,
            device_idx);

        _impl->loader = std::move(loader);
        _impl->loaded_path = path;
        return true;

    } catch (c10::Error const & e) {
        std::cerr << "[AOTInductorBackend] c10 error loading " << path
                  << ": " << e.what() << "\n";
        _impl->loader.reset();
        return false;
    } catch (std::exception const & e) {
        std::cerr << "[AOTInductorBackend] Exception loading " << path
                  << ": " << e.what() << "\n";
        _impl->loader.reset();
        return false;
    }
}

// ---------------------------------------------------------------------------
// isLoaded / loadedPath
// ---------------------------------------------------------------------------
bool AOTInductorBackend::isLoaded() const
{
    return _impl->loader != nullptr;
}

std::filesystem::path AOTInductorBackend::loadedPath() const
{
    return _impl->loaded_path;
}

// ---------------------------------------------------------------------------
// execute (default method)
// ---------------------------------------------------------------------------
std::vector<torch::Tensor>
AOTInductorBackend::execute(std::vector<torch::Tensor> const & inputs)
{
    if (!isLoaded()) {
        throw std::runtime_error("[AOTInductorBackend] No model loaded");
    }

    auto & dm = DeviceManager::instance();

    // Move inputs to the correct device
    std::vector<at::Tensor> device_inputs;
    device_inputs.reserve(inputs.size());
    for (auto const & t : inputs) {
        device_inputs.push_back(dm.toDevice(t));
    }

    try {
        return _impl->loader->run(device_inputs);

    } catch (c10::Error const & e) {
        std::ostringstream oss;
        oss << "[AOTInductorBackend] Execution failed: " << e.what() << "\n"
            << "  Number of inputs: " << inputs.size() << "\n";
        for (size_t i = 0; i < inputs.size(); ++i) {
            oss << "    Input[" << i << "]: shape=" << inputs[i].sizes()
                << ", dtype=" << inputs[i].dtype()
                << ", device=" << inputs[i].device() << "\n";
        }
        throw std::runtime_error(oss.str());
    }
}

// ---------------------------------------------------------------------------
// execute (named method — AOT Inductor only has one entry point)
// ---------------------------------------------------------------------------
std::vector<torch::Tensor>
AOTInductorBackend::execute(std::string const & /*method_name*/,
                            std::vector<torch::Tensor> const & inputs)
{
    // AOT Inductor packages contain a single compiled model; method_name is ignored.
    return execute(inputs);
}

} // namespace dl
