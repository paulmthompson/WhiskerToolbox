/**
 * @file EncoderShapeWidget.hpp
 * @brief Self-contained widget for configuring model-specific shape parameters.
 *
 * Schema-driven form powered by AutoParamWidget. Configuration is persisted
 * in DeepLearningState::model_configurations and applied via ModelRegistry.
 */

#ifndef ENCODER_SHAPE_WIDGET_HPP
#define ENCODER_SHAPE_WIDGET_HPP

#include <QWidget>

#include <memory>
#include <string>

class AutoParamWidget;
class DeepLearningState;
class QGroupBox;
class QPushButton;
class SlotAssembler;

namespace dl::widget {

/**
 * @brief Widget for configuring a model's registered configuration parameters.
 *
 * Wraps an AutoParamWidget driven by the active model's ParameterSchema.
 * An Apply button pushes configuration to SlotAssembler through the registry.
 *
 * @note Not thread-safe — must be used from the GUI thread only.
 */
class EncoderShapeWidget : public QWidget {
    Q_OBJECT

public:
    /**
     * @brief Construct the model configuration widget.
     * @param state     DeepLearningState for model_configurations persistence.
     * @param assembler SlotAssembler for applyModelConfiguration.
     * @param parent    Optional parent widget.
     *
     * @pre state and assembler must not be null.
     */
    explicit EncoderShapeWidget(
            std::shared_ptr<DeepLearningState> state,
            SlotAssembler * assembler,
            QWidget * parent = nullptr);

    ~EncoderShapeWidget() override;

    EncoderShapeWidget(EncoderShapeWidget const &) = delete;
    EncoderShapeWidget & operator=(EncoderShapeWidget const &) = delete;
    EncoderShapeWidget(EncoderShapeWidget &&) = delete;
    EncoderShapeWidget & operator=(EncoderShapeWidget &&) = delete;

    /**
     * @brief Apply current state configuration to SlotAssembler (for model load).
     */
    void applyFromState();

    /**
     * @brief Apply active model configuration from state to assembler (static).
     */
    static void applyConfigurationFromState(
            DeepLearningState const * state,
            SlotAssembler * assembler);

signals:
    void bindingChanged();
    void configurationApplied();

private:
    void _syncToState();
    void _onApplyClicked();

    std::shared_ptr<DeepLearningState> _state;
    SlotAssembler * _assembler;

    AutoParamWidget * _auto_param = nullptr;
    QPushButton * _apply_btn = nullptr;
};

}// namespace dl::widget

#endif// ENCODER_SHAPE_WIDGET_HPP
