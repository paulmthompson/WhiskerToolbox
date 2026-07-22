/**
 * @file RuntimeModel.hpp
 * @brief ModelBase implementation driven by a JSON RuntimeModelSpec.
 */

#ifndef NEURALYZER_RUNTIME_MODEL_HPP
#define NEURALYZER_RUNTIME_MODEL_HPP

#include "RuntimeModelSpec.hpp"
#include "models_v2/ModelBase.hpp"

#include <ATen/core/Tensor.h>// at::Tensor

#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

namespace dl {

class ModelExecution;

/**
 * @brief A ModelBase implementation driven entirely by a RuntimeModelSpec (JSON).
 *
 * RuntimeModel allows users to define model inputs, outputs, and metadata
 * via a JSON specification file, without recompiling. It uses ModelExecution
 * to load and run ExecuTorch `.pte` programs.
 *
 * Usage:
 * @code
 *     auto spec_result = RuntimeModelSpec::fromJsonFile("model.json");
 *     if (spec_result) {
 *         auto model = std::make_unique<RuntimeModel>(std::move(spec_result.value()));
 *         model->loadWeights("/path/to/model.pte");
 *         // ... use like any ModelBase ...
 *     }
 * @endcode
 */
class RuntimeModel : public ModelBase {
public:
    /**
     * @brief Construct from a parsed spec.
     *
     * If the spec contains a `weights_path`, the model will attempt to load
     * weights from that path during construction.
     */
    explicit RuntimeModel(RuntimeModelSpec spec);

    ~RuntimeModel() override;

    [[nodiscard]] std::string modelId() const override;
    [[nodiscard]] std::string displayName() const override;
    [[nodiscard]] std::string description() const override;
    [[nodiscard]] std::vector<TensorSlotDescriptor> inputSlots() const override;
    [[nodiscard]] std::vector<TensorSlotDescriptor> outputSlots() const override;
    [[nodiscard]] std::string recommendedPostEncoderModule() const override;

    void loadWeights(std::filesystem::path const & path) override;
    [[nodiscard]] bool isReady() const override;

    [[nodiscard]] int preferredBatchSize() const override;
    [[nodiscard]] int maxBatchSize() const override;
    [[nodiscard]] dl::BatchMode batchMode() const override;

    /**
     * @brief Select and load the best weights variant for the given batch size.
     *
     * Searches `weights_variants` for an exact batch_size match. If none
     * is found, falls back to the default `weights_path`.
     *
     * @return true if a variant was found and loaded successfully
     */
    bool loadWeightsForBatchSize(int batch_size);

    /**
     * @brief Get the list of available weights variants (if any).
     */
    [[nodiscard]] std::vector<WeightsVariant> weightsVariants() const;

    std::unordered_map<std::string, at::Tensor>
    forward(std::unordered_map<std::string, at::Tensor> const & inputs) override;

    /**
     * @brief Access the underlying spec.
     */
    [[nodiscard]] RuntimeModelSpec const & spec() const;

private:
    RuntimeModelSpec _spec;
    std::vector<TensorSlotDescriptor> _input_slots;
    std::vector<TensorSlotDescriptor> _output_slots;
    /** @brief Ordered input names for executeNamed(). */
    std::vector<std::string> _input_order;
    /** @brief Ordered output names. */
    std::vector<std::string> _output_order;
    std::unique_ptr<ModelExecution> _execution;
};

}// namespace dl

#endif// NEURALYZER_RUNTIME_MODEL_HPP
