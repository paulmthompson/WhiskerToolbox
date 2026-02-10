#include "ModelExecution.hpp"

#include <executorch/extension/module/module.h>
#include <executorch/extension/tensor/tensor_ptr.h>
#include <executorch/runtime/core/evalue.h>
#include <executorch/runtime/core/exec_aten/exec_aten.h>

#include <iostream>
#include <stdexcept>
#include <utility>

namespace dl {

// ---------------------------------------------------------------------------
// Helper: convert at::Tensor → ExecuTorch EValue (aliasing data, no copy)
// ---------------------------------------------------------------------------
namespace {

executorch::runtime::EValue atTensorToEValue(torch::Tensor const & at_tensor)
{
    // Ensure contiguous CPU tensor for ExecuTorch
    auto contig = at_tensor.contiguous().to(torch::kCPU);

    // Map sizes from int64_t → ExecuTorch SizesType (int32_t without USE_ATEN_LIB)
    auto const at_sizes = contig.sizes();
    std::vector<executorch::aten::SizesType> et_sizes(at_sizes.begin(), at_sizes.end());

    // Map scalar type
    executorch::aten::ScalarType et_dtype = executorch::aten::ScalarType::Float;
    switch (contig.scalar_type()) {
        case torch::kFloat32: et_dtype = executorch::aten::ScalarType::Float; break;
        case torch::kFloat64: et_dtype = executorch::aten::ScalarType::Double; break;
        case torch::kInt32:   et_dtype = executorch::aten::ScalarType::Int; break;
        case torch::kInt64:   et_dtype = executorch::aten::ScalarType::Long; break;
        case torch::kBool:    et_dtype = executorch::aten::ScalarType::Bool; break;
        case torch::kByte:    et_dtype = executorch::aten::ScalarType::Byte; break;
        default:
            throw std::runtime_error(
                "Unsupported torch dtype for ExecuTorch conversion: " +
                std::string(toString(contig.scalar_type())));
    }

    // Create ExecuTorch TensorPtr aliasing the at::Tensor data.
    // The at::Tensor must outlive the TensorPtr — caller is responsible.
    auto tensor_ptr = executorch::extension::make_tensor_ptr(
        std::move(et_sizes),
        contig.data_ptr(),
        et_dtype);

    return executorch::runtime::EValue(std::move(*tensor_ptr));
}

// ---------------------------------------------------------------------------
// Helper: convert ExecuTorch Tensor in an EValue → at::Tensor (clone, safe)
// ---------------------------------------------------------------------------
torch::Tensor eValueToAtTensor(executorch::runtime::EValue & ev)
{
    if (!ev.isTensor()) {
        throw std::runtime_error("EValue is not a Tensor");
    }
    auto const & et_tensor = ev.toTensor();

    // Map to at:: scalar type
    at::ScalarType at_dtype = at::kFloat;
    switch (et_tensor.scalar_type()) {
        case executorch::aten::ScalarType::Float:  at_dtype = at::kFloat; break;
        case executorch::aten::ScalarType::Double: at_dtype = at::kDouble; break;
        case executorch::aten::ScalarType::Int:    at_dtype = at::kInt; break;
        case executorch::aten::ScalarType::Long:   at_dtype = at::kLong; break;
        case executorch::aten::ScalarType::Bool:   at_dtype = at::kBool; break;
        case executorch::aten::ScalarType::Byte:   at_dtype = at::kByte; break;
        default:
            throw std::runtime_error("Unsupported ExecuTorch dtype for at::Tensor conversion");
    }

    // Build an at::Tensor aliasing the ExecuTorch tensor's data, then clone
    // so the output is independent of ExecuTorch's internal buffers.
    auto const ndim = et_tensor.dim();
    std::vector<int64_t> sizes(ndim);
    for (ssize_t i = 0; i < ndim; ++i) {
        sizes[static_cast<size_t>(i)] = et_tensor.size(i);
    }

    auto options = torch::TensorOptions().dtype(at_dtype).device(torch::kCPU);
    auto alias = torch::from_blob(
        const_cast<void *>(et_tensor.const_data_ptr()),
        sizes,
        options);

    return alias.clone();
}

} // anonymous namespace

// ---------------------------------------------------------------------------
// PIMPL
// ---------------------------------------------------------------------------
struct ModelExecution::Impl {
    std::unique_ptr<executorch::extension::module::Module> module;
    std::filesystem::path loaded_path;
};

// ---------------------------------------------------------------------------
// Construction / destruction / move
// ---------------------------------------------------------------------------
ModelExecution::ModelExecution()
    : _impl(std::make_unique<Impl>())
{
}

ModelExecution::~ModelExecution() = default;
ModelExecution::ModelExecution(ModelExecution &&) noexcept = default;
ModelExecution & ModelExecution::operator=(ModelExecution &&) noexcept = default;

// ---------------------------------------------------------------------------
// load
// ---------------------------------------------------------------------------
bool ModelExecution::load(std::filesystem::path const & pte_path)
{
    try {
        auto mod = std::make_unique<executorch::extension::module::Module>(
            pte_path.string(),
            executorch::extension::module::Module::LoadMode::File);

        auto err = mod->load();
        if (err != executorch::runtime::Error::Ok) {
            std::cerr << "[ModelExecution] Failed to load program: "
                      << pte_path << "\n";
            return false;
        }

        _impl->module = std::move(mod);
        _impl->loaded_path = pte_path;
        return true;

    } catch (std::exception const & e) {
        std::cerr << "[ModelExecution] Exception loading " << pte_path
                  << ": " << e.what() << "\n";
        return false;
    }
}

// ---------------------------------------------------------------------------
// isLoaded / loadedPath
// ---------------------------------------------------------------------------
bool ModelExecution::isLoaded() const
{
    return _impl->module && _impl->module->is_loaded();
}

std::filesystem::path ModelExecution::loadedPath() const
{
    return _impl->loaded_path;
}

// ---------------------------------------------------------------------------
// execute (forward method, ordered inputs)
// ---------------------------------------------------------------------------
std::vector<torch::Tensor>
ModelExecution::execute(std::vector<torch::Tensor> const & inputs)
{
    return execute("forward", inputs);
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

    // Keep contiguous inputs alive for the duration of execution
    std::vector<torch::Tensor> kept_alive;
    kept_alive.reserve(inputs.size());

    // Convert at::Tensor → EValue
    std::vector<executorch::runtime::EValue> evalues;
    evalues.reserve(inputs.size());
    for (auto const & t : inputs) {
        kept_alive.push_back(t.contiguous().to(torch::kCPU));
        evalues.push_back(atTensorToEValue(kept_alive.back()));
    }

    // Execute
    auto result = _impl->module->execute(method_name, evalues);
    if (!result.ok()) {
        throw std::runtime_error(
            "[ModelExecution] Execution of method '" + method_name + "' failed");
    }

    // Convert output EValues → at::Tensor
    auto & output_evalues = result.get();
    std::vector<torch::Tensor> outputs;
    outputs.reserve(output_evalues.size());
    for (auto & ev : output_evalues) {
        if (ev.isTensor()) {
            outputs.push_back(eValueToAtTensor(ev));
        }
        // Skip non-tensor outputs (e.g. None, scalars)
    }

    return outputs;
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
