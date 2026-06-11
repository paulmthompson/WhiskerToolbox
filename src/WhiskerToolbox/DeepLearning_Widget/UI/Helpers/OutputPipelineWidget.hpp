/**
 * @file OutputPipelineWidget.hpp
 * @brief Per-output editor for tensor transform and terminal decoder pipelines.
 */

#ifndef OUTPUT_PIPELINE_WIDGET_HPP
#define OUTPUT_PIPELINE_WIDGET_HPP

#include "DeepLearning_Widget/Core/DeepLearningBindingData.hpp"

#include "models_v2/TensorSlotDescriptor.hpp"

#include <QWidget>

#include <memory>
#include <string>
#include <vector>

class DataManager;
class QCheckBox;
class QComboBox;
class QDoubleSpinBox;
class QGroupBox;
class QLabel;

namespace dl::widget {

/**
 * @brief Widget that presents one ordered output pipeline for a model output.
 *
 * The UI shows tensor transforms before the terminal decoder so the visual order
 * matches execution. The saved representation remains pure data.
 *
 * @pre dm must not be null.
 */
class OutputPipelineWidget : public QWidget {
    Q_OBJECT

public:
    /**
     * @brief Construct a pipeline widget for one output slot.
     *
     * @param model_id Model id used for default pipeline selection.
     * @param slot Output tensor slot descriptor.
     * @param dm DataManager used to populate output target and point-source combos.
     * @param parent Optional parent widget.
     *
     * @pre dm must not be null.
     */
    explicit OutputPipelineWidget(std::string model_id,
                                  dl::TensorSlotDescriptor slot,
                                  std::shared_ptr<DataManager> dm,
                                  QWidget * parent = nullptr);

    ~OutputPipelineWidget() override;

    OutputPipelineWidget(OutputPipelineWidget const &) = delete;
    OutputPipelineWidget & operator=(OutputPipelineWidget const &) = delete;
    OutputPipelineWidget(OutputPipelineWidget &&) = delete;
    OutputPipelineWidget & operator=(OutputPipelineWidget &&) = delete;

    /**
     * @brief Return the model output slot name.
     */
    [[nodiscard]] std::string const & slotName() const;

    /**
     * @brief Convert the current UI state to serializable binding data.
     */
    [[nodiscard]] OutputBindingData toOutputBindingData() const;

    /**
     * @brief Restore the UI from saved binding data.
     */
    void setBinding(OutputBindingData const & binding);

    /**
     * @brief Refresh DataManager-backed combo boxes.
     */
    void refreshDataSources();

signals:
    /**
     * @brief Emitted whenever the selected pipeline or target changes.
     */
    void bindingChanged();

private:
    void _buildUi();
    void _populateTransformCombo();
    void _populateDecoderCombo(std::vector<int64_t> const & tensor_shape);
    void _refreshPointCombo();
    void _refreshTargetCombo();
    void _syncParameterVisibility();
    void _updatePipelineSummary();
    void _onPipelineShapeChanged();

    [[nodiscard]] std::vector<dl::OutputPipelineStepSpec> _currentPipeline() const;
    [[nodiscard]] std::vector<int64_t> _shapeAfterTransform() const;
    [[nodiscard]] std::string _selectedTransformId() const;
    [[nodiscard]] std::string _selectedDecoderId() const;
    [[nodiscard]] static QString _shapeText(std::vector<int64_t> const & shape);
    [[nodiscard]] static QString _displayName(std::string const & step_id);

    std::string _model_id;
    dl::TensorSlotDescriptor _slot;
    std::shared_ptr<DataManager> _dm;

    QGroupBox * _group = nullptr;
    QLabel * _input_shape_label = nullptr;
    QLabel * _pipeline_summary_label = nullptr;
    QComboBox * _transform_combo = nullptr;
    QComboBox * _spatial_point_combo = nullptr;
    QComboBox * _interpolation_combo = nullptr;
    QComboBox * _decoder_combo = nullptr;
    QDoubleSpinBox * _threshold_spin = nullptr;
    QCheckBox * _subpixel_check = nullptr;
    QComboBox * _target_combo = nullptr;
};

}// namespace dl::widget

#endif// OUTPUT_PIPELINE_WIDGET_HPP
