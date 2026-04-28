/**
 * @file EncoderShapeWidget.hpp
 * @brief Self-contained widget for configuring encoder input/output shape.
 * 
 * Replaces the hand-built `_buildEncoderShapeSection()` panel with a
 * schema-driven form powered by AutoParamWidget. The widget owns an
 * `EncoderShapeParams` struct, syncs changes to DeepLearningState, and
 * applies shape configuration to SlotAssembler when the user clicks Apply.
 */

#ifndef ENCODER_SHAPE_WIDGET_HPP
#define ENCODER_SHAPE_WIDGET_HPP

#include "DeepLearning_Widget/Core/DeepLearningParamSchemas.hpp"

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
 * @brief Widget for configuring encoder input resolution and output shape.
 *
 * Wraps an AutoParamWidget driven by the `EncoderShapeParams` schema.
 * Provides input height/width spin boxes and output shape text field.
 * An Apply button pushes the configuration to SlotAssembler.
 *
 * @note Not thread-safe — must be used from the GUI thread only.
 */
class EncoderShapeWidget : public QWidget {
    Q_OBJECT

public:
    /**
     * @brief Construct the encoder shape widget.
     * @param state     DeepLearningState to sync height, width, output_shape.
     * @param assembler SlotAssembler for configureModelShape.
     * @param parent    Optional parent widget.
     *
     * @pre state and assembler must not be null.
     */
    explicit EncoderShapeWidget(
            std::shared_ptr<DeepLearningState> state,
            SlotAssembler * assembler,
            QWidget * parent = nullptr);

    ~EncoderShapeWidget() override;

    // Non-copyable, non-movable (QWidget)
    EncoderShapeWidget(EncoderShapeWidget const &) = delete;
    EncoderShapeWidget & operator=(EncoderShapeWidget const &) = delete;
    EncoderShapeWidget(EncoderShapeWidget &&) = delete;
    EncoderShapeWidget & operator=(EncoderShapeWidget &&) = delete;

    /**
     * @brief Return the current parameter values.
     */
    [[nodiscard]] EncoderShapeParams params() const;

    /**
     * @brief Set the parameter values and update the UI.
     */
    void setParams(EncoderShapeParams const & params);

    /**
     * @brief Apply current state to SlotAssembler (for model load restore).
     * 
     * Reads from state, calls configureModelShape, does not emit shapeApplied.
     */
    void applyFromState();

    /**
     * @brief Apply encoder shape from state to assembler (static, for model load).
     * 
     * Call when loading a general_encoder model before the widget exists.
     */
    static void applyEncoderShapeToAssembler(
            DeepLearningState const * state,
            SlotAssembler * assembler);

    /// @brief Build EncoderShapeParams from saved state (for restore).
    [[nodiscard]] static EncoderShapeParams paramsFromState(
            int input_height,
            int input_width,
            std::string const & output_shape);

signals:
    /// Emitted whenever any parameter in the encoder shape section changes.
    void bindingChanged();

    /// Emitted after Apply button applies shape to assembler.
    /// Parent should refresh model info and rebuild slot panels.
    void shapeApplied();

private:
    /// Sync current params to state.
    void _syncToState();

    /// Apply current params to assembler and emit shapeApplied.
    void _onApplyClicked();

    std::shared_ptr<DeepLearningState> _state;
    SlotAssembler * _assembler;

    AutoParamWidget * _auto_param = nullptr;
    QPushButton * _apply_btn = nullptr;
};

}// namespace dl::widget

#endif// ENCODER_SHAPE_WIDGET_HPP
