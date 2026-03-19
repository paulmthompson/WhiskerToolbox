#ifndef DEEP_LEARNING_PROPERTIES_WIDGET_HPP
#define DEEP_LEARNING_PROPERTIES_WIDGET_HPP

/**
 * @file DeepLearningPropertiesWidget.hpp
 * @brief Properties panel for configuring deep learning model inference.
 *
 * Provides model selection, weights loading, dynamic input/output slot
 * binding to DataManager keys, static (memory) input configuration,
 * and run controls. The form is rebuilt dynamically whenever the user
 * selects a different model.
 *
 * All libtorch interactions are routed through SlotAssembler (PIMPL)
 * so this header and translation unit never include <torch/torch.h>.
 */

#include "Core/SlotAssembler.hpp"// SlotAssembler, ModelDisplayInfo

#include "TimeFrame/TimeFrame.hpp"           // TimePosition
#include "models_v2/TensorSlotDescriptor.hpp"// TensorSlotDescriptor, ModelDisplayInfo

#include <QWidget>

#include <memory>
#include <optional>
#include <string>
#include <utility>
#include <vector>

class QComboBox;
class QDoubleSpinBox;
class QGroupBox;
class QLabel;
class QLineEdit;
class QPushButton;
class QSpinBox;
class QVBoxLayout;

class DataManager;
class DeepLearningState;
class InferenceController;

namespace dl::widget {
class DynamicInputSlotWidget;
class EncoderShapeWidget;
class OutputSlotWidget;
class PostEncoderWidget;
class RecurrentBindingWidget;
class SequenceSlotWidget;
class StaticInputSlotWidget;
}// namespace dl::widget

class DeepLearningPropertiesWidget : public QWidget {
    Q_OBJECT

public:
    explicit DeepLearningPropertiesWidget(
            std::shared_ptr<DeepLearningState> state,
            std::shared_ptr<DataManager> data_manager,
            QWidget * parent = nullptr);

    ~DeepLearningPropertiesWidget() override;

    /// Non-owning access to the SlotAssembler for cache preview queries.
    [[nodiscard]] SlotAssembler * assembler() const;

public slots:
    /**
     * @brief Handle time changes from EditorRegistry
     * 
     * Updates the current time position for the "Predict Current Frame" feature.
     * 
     * @param position The new TimePosition
     */
    void onTimeChanged(TimePosition const & position);

signals:
    /// Emitted when the static tensor cache changes (capture/clear).
    void staticCacheChanged();

    /// Emitted during recurrent inference to report progress.
    /// @param current 0-based frame being processed
    /// @param total Total number of frames
    void recurrentProgressChanged(int current, int total);

    /// Emitted during batch inference to report progress.
    /// @param current 0-based index of the frame being processed
    /// @param total Total number of frames to process
    void batchProgressChanged(int current, int total);

private slots:
    void _onModelComboChanged(int index);
    void _onWeightsBrowseClicked();
    void _onWeightsPathEdited();
    void _onRunSingleFrame();
    void _onRunBatch();
    void _onRunRecurrentSequence();
    void _onPredictCurrentFrame();
    void _onCaptureStaticInput(std::string const & slot_name);
    void _onCaptureSequenceEntry(std::string const & slot_name,
                                 int memory_index);
    void _onInferenceBatchFinished(bool success, QString const & error_message);
    void _onInferenceRunningChanged(bool running);

private:
    void _buildUi();
    void _populateModelCombo();
    void _rebuildSlotPanels();
    void _clearDynamicContent();

    QGroupBox * _buildBooleanMaskGroup(dl::TensorSlotDescriptor const & slot);

    void _populateDataSourceCombo(QComboBox * combo,
                                  std::string const & type_hint);
    void _refreshDataSourceCombos();

    void _syncBindingsFromUi();
    void _updateWeightsStatus();
    void _loadModelIfReady();
    void _updateBatchSizeConstraint();
    void _enforcePostEncoderDecoderConsistency();

    std::shared_ptr<DeepLearningState> _state;
    std::shared_ptr<DataManager> _data_manager;

    // Fixed UI elements
    QComboBox * _model_combo = nullptr;
    QLabel * _model_desc_label = nullptr;
    QLineEdit * _weights_path_edit = nullptr;
    QPushButton * _weights_browse_btn = nullptr;
    QLabel * _weights_status_label = nullptr;
    QSpinBox * _frame_spin = nullptr;
    QSpinBox * _batch_size_spin = nullptr;
    QPushButton * _run_single_btn = nullptr;
    QPushButton * _run_batch_btn = nullptr;
    QPushButton * _run_recurrent_btn = nullptr;
    QPushButton * _predict_current_frame_btn = nullptr;

    // Dynamic content container
    QVBoxLayout * _dynamic_layout = nullptr;
    QWidget * _dynamic_container = nullptr;

    // SlotAssembler owns the model behind a PIMPL firewall.
    std::unique_ptr<SlotAssembler> _assembler;

    // Dynamic input slot widgets (non-owning; owned by _dynamic_container)
    std::vector<dl::widget::DynamicInputSlotWidget *> _dynamic_input_widgets;

    // Static input slot widgets (non-owning; owned by _dynamic_container)
    std::vector<dl::widget::StaticInputSlotWidget *> _static_input_widgets;

    // Sequence slot widgets (non-owning; owned by _dynamic_container)
    std::vector<dl::widget::SequenceSlotWidget *> _sequence_slot_widgets;

    // Output slot widgets (non-owning; owned by _dynamic_container)
    std::vector<dl::widget::OutputSlotWidget *> _output_slot_widgets;

    // Post-encoder widget (non-owning; owned by _dynamic_container)
    dl::widget::PostEncoderWidget * _post_encoder_widget = nullptr;

    // Encoder shape widget (non-owning; owned by _dynamic_container)
    dl::widget::EncoderShapeWidget * _encoder_shape_widget = nullptr;

    // Recurrent binding widgets (non-owning; owned by _dynamic_container)
    std::vector<dl::widget::RecurrentBindingWidget *> _recurrent_binding_widgets;

    // Cached model display info (clean — no torch types).
    std::optional<ModelDisplayInfo> _current_info;

    // Last weight-validation result; empty means validated OK.
    std::string _weights_validation_error;

    // Current time position from EditorRegistry
    std::optional<TimePosition> _current_time_position;

    // DataManager observer for data add/delete notifications
    int _dm_observer_id = -1;

    // Inference orchestration (owns worker thread, result merging).
    std::unique_ptr<InferenceController> _inference_controller;
};

#endif// DEEP_LEARNING_PROPERTIES_WIDGET_HPP
