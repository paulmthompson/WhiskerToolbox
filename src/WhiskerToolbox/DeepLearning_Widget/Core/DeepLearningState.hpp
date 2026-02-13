#ifndef DEEP_LEARNING_STATE_HPP
#define DEEP_LEARNING_STATE_HPP

/**
 * @file DeepLearningState.hpp
 * @brief State class for the DeepLearning widget.
 *
 * Stores complete configuration for a deep learning inference session:
 * selected model, weights path, input/output slot bindings, static
 * memory inputs, and batch parameters.
 *
 * Serialized via reflect-cpp for workspace save/restore.
 *
 * @see EditorState for base class documentation.
 * @see SlotAssembler for the DataManager-to-tensor bridge.
 */

#include "DeepLearningBindingData.hpp"

#include "EditorState/EditorState.hpp"

#include <memory>
#include <string>
#include <vector>

/// Aggregate data for reflect-cpp serialization.
struct DeepLearningStateData {
    std::string selected_model_id;
    std::string weights_path;
    int batch_size = 1;
    int current_frame = 0;
    std::vector<SlotBindingData> input_bindings;
    std::vector<OutputBindingData> output_bindings;
    std::vector<StaticInputData> static_inputs;
    std::string instance_id;
    std::string display_name = "Deep Learning";
};

/**
 * @brief State class for the DeepLearning widget.
 *
 * Stores model selection, slot bindings, weights path, batch size, and
 * frame position. All changes emit typed signals so both the View and
 * Properties panels stay in sync.
 */
class DeepLearningState : public EditorState {
    Q_OBJECT

public:
    explicit DeepLearningState(QObject * parent = nullptr);
    ~DeepLearningState() override = default;

    [[nodiscard]] QString getTypeName() const override;
    [[nodiscard]] std::string toJson() const override;
    bool fromJson(std::string const & json) override;

    // ── Model Selection ──
    [[nodiscard]] std::string const & selectedModelId() const;
    void setSelectedModelId(std::string const & id);

    // ── Weights Path ──
    [[nodiscard]] std::string const & weightsPath() const;
    void setWeightsPath(std::string const & path);

    // ── Batch Size ──
    [[nodiscard]] int batchSize() const;
    void setBatchSize(int size);

    // ── Current Frame ──
    [[nodiscard]] int currentFrame() const;
    void setCurrentFrame(int frame);

    // ── Input Bindings ──
    [[nodiscard]] std::vector<SlotBindingData> const & inputBindings() const;
    void setInputBindings(std::vector<SlotBindingData> bindings);

    // ── Output Bindings ──
    [[nodiscard]] std::vector<OutputBindingData> const & outputBindings() const;
    void setOutputBindings(std::vector<OutputBindingData> bindings);

    // ── Static Inputs ──
    [[nodiscard]] std::vector<StaticInputData> const & staticInputs() const;
    void setStaticInputs(std::vector<StaticInputData> inputs);

signals:
    void modelChanged();
    void weightsPathChanged();
    void batchSizeChanged(int size);
    void currentFrameChanged(int frame);
    void inputBindingsChanged();
    void outputBindingsChanged();
    void staticInputsChanged();

private:
    DeepLearningStateData _data;
};

#endif // DEEP_LEARNING_STATE_HPP
