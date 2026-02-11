#ifndef WHISKERTOOLBOX_RUNTIME_MODEL_HPP
#define WHISKERTOOLBOX_RUNTIME_MODEL_HPP

#include "RuntimeModelSpec.hpp"
#include "models_v2/ModelBase.hpp"
#include "models_v2/ModelExecution.hpp"
#include "models_v2/backends/InferenceBackend.hpp"

#include <filesystem>
#include <string>
#include <unordered_map>
#include <vector>

namespace dl {

/// A ModelBase implementation driven entirely by a RuntimeModelSpec (JSON).
///
/// RuntimeModel allows users to define model inputs, outputs, and metadata
/// via a JSON specification file, without recompiling. It uses ModelExecution
/// to load and run ExecuTorch `.pte` programs.
///
/// Usage:
/// @code
///     auto spec_result = RuntimeModelSpec::fromJsonFile("model.json");
///     if (spec_result) {
///         auto model = std::make_unique<RuntimeModel>(std::move(spec_result.value()));
///         model->loadWeights("/path/to/model.pte");
///         // ... use like any ModelBase ...
///     }
/// @endcode
class RuntimeModel : public ModelBase {
public:
    /// Construct from a parsed spec.
    ///
    /// If the spec contains a `weights_path`, the model will attempt to load
    /// weights from that path during construction.
    explicit RuntimeModel(RuntimeModelSpec spec);

    [[nodiscard]] std::string modelId() const override;
    [[nodiscard]] std::string displayName() const override;
    [[nodiscard]] std::string description() const override;
    [[nodiscard]] std::vector<TensorSlotDescriptor> inputSlots() const override;
    [[nodiscard]] std::vector<TensorSlotDescriptor> outputSlots() const override;

    void loadWeights(std::filesystem::path const & path) override;
    [[nodiscard]] bool isReady() const override;

    [[nodiscard]] int preferredBatchSize() const override;
    [[nodiscard]] int maxBatchSize() const override;

    std::unordered_map<std::string, torch::Tensor>
    forward(std::unordered_map<std::string, torch::Tensor> const & inputs) override;

    /// Access the underlying spec.
    [[nodiscard]] RuntimeModelSpec const & spec() const;

private:
    RuntimeModelSpec _spec;
    std::vector<TensorSlotDescriptor> _input_slots;
    std::vector<TensorSlotDescriptor> _output_slots;
    std::vector<std::string> _input_order;   ///< Ordered input names for executeNamed()
    std::vector<std::string> _output_order;  ///< Ordered output names
    ModelExecution _execution;
};

} // namespace dl

#endif // WHISKERTOOLBOX_RUNTIME_MODEL_HPP
