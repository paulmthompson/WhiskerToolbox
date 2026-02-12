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

#include "DeepLearning_Widget/Core/SlotAssembler.hpp"
#include "TimeFrame/TimeFrame.hpp"

#include <QWidget>

#include <memory>
#include <optional>
#include <string>
#include <vector>

// Clean forward declarations — no libtorch dependency.
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

class DeepLearningPropertiesWidget : public QWidget {
    Q_OBJECT

public:
    explicit DeepLearningPropertiesWidget(
        std::shared_ptr<DeepLearningState> state,
        std::shared_ptr<DataManager> data_manager,
        QWidget * parent = nullptr);

    ~DeepLearningPropertiesWidget() override;

public slots:
    /**
     * @brief Handle time changes from EditorRegistry
     * 
     * Updates the current time position for the "Predict Current Frame" feature.
     * 
     * @param position The new TimePosition
     */
    void onTimeChanged(TimePosition position);

private slots:
    void _onModelComboChanged(int index);
    void _onWeightsBrowseClicked();
    void _onWeightsPathEdited();
    void _onRunSingleFrame();
    void _onRunBatch();
    void _onPredictCurrentFrame();

private:
    void _buildUi();
    void _populateModelCombo();
    void _rebuildSlotPanels();
    void _clearDynamicContent();

    QGroupBox * _buildDynamicInputGroup(dl::TensorSlotDescriptor const & slot);
    QGroupBox * _buildStaticInputGroup(dl::TensorSlotDescriptor const & slot);
    QGroupBox * _buildBooleanMaskGroup(dl::TensorSlotDescriptor const & slot);
    QGroupBox * _buildOutputGroup(dl::TensorSlotDescriptor const & slot);

    void _populateDataSourceCombo(QComboBox * combo,
                                  std::string const & type_hint);
    [[nodiscard]] std::vector<std::string> _modesForEncoder(
        std::string const & encoder_id) const;

    void _syncBindingsFromUi();
    void _updateWeightsStatus();
    void _loadModelIfReady();

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
    QPushButton * _predict_current_frame_btn = nullptr;

    // Dynamic content container
    QVBoxLayout * _dynamic_layout = nullptr;
    QWidget * _dynamic_container = nullptr;

    // SlotAssembler owns the model behind a PIMPL firewall.
    std::unique_ptr<SlotAssembler> _assembler;

    // Cached model display info (clean — no torch types).
    std::optional<ModelDisplayInfo> _current_info;

    // Current time position from EditorRegistry
    std::optional<TimePosition> _current_time_position;
};

#endif // DEEP_LEARNING_PROPERTIES_WIDGET_HPP
