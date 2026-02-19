#ifndef FEATURE_SELECTION_PANEL_HPP
#define FEATURE_SELECTION_PANEL_HPP

/**
 * @file FeatureSelectionPanel.hpp
 * @brief Panel for selecting a TensorData feature matrix in the ML workflow
 *
 * FeatureSelectionPanel displays a combo box listing all TensorData keys in
 * the DataManager, along with summary information about the selected tensor:
 *
 * - **Column names** (feature names from the tensor's DimensionDescriptor)
 * - **Row count** and **column count** (shape)
 * - **Row type** (TimeFrameIndex, Interval, or Ordinal) from RowDescriptor
 * - **Time frame** name (TimeKey) for tensors with temporal row semantics
 *
 * The panel validates that a tensor is selected and non-empty. Downstream
 * panels can check validity before proceeding with training/prediction.
 *
 * ## Integration
 *
 * - Embedded in the Classification tab of MLCoreWidget
 * - Reads/writes `feature_tensor_key` in MLCoreWidgetState
 * - Refreshes automatically when DataManager contents change
 * - Shows a hint linking to TensorDesigner for building new tensors
 *
 * @see MLCoreWidget for the parent widget
 * @see MLCoreWidgetState::featureTensorKey for the persisted selection
 * @see TensorData for the data type being selected
 * @see RowDescriptor for row type and time frame information
 */

#include <QWidget>

#include <memory>
#include <string>

// Forward declarations
class DataManager;
class MLCoreWidgetState;

namespace Ui {
class FeatureSelectionPanel;
}

class FeatureSelectionPanel : public QWidget {
    Q_OBJECT

public:
    /**
     * @brief Construct the feature selection panel
     *
     * @param state Shared MLCoreWidgetState for reading/writing feature_tensor_key
     * @param data_manager Shared DataManager for listing TensorData keys
     * @param parent Optional parent widget
     */
    explicit FeatureSelectionPanel(std::shared_ptr<MLCoreWidgetState> state,
                                   std::shared_ptr<DataManager> data_manager,
                                   QWidget * parent = nullptr);

    ~FeatureSelectionPanel() override;

    /**
     * @brief Whether a valid tensor is currently selected
     *
     * Returns true if the combo box has a non-empty selection that resolves
     * to a non-null TensorData with at least one row and one column.
     */
    [[nodiscard]] bool hasValidSelection() const;

    /**
     * @brief Get the currently selected tensor key
     *
     * @return The DataManager key of the selected TensorData, or empty string
     */
    [[nodiscard]] std::string selectedTensorKey() const;

public slots:
    /**
     * @brief Refresh the list of available TensorData keys from DataManager
     *
     * Called automatically on DataManager changes and on the refresh button.
     * Preserves the current selection if still available.
     */
    void refreshTensorList();

signals:
    /**
     * @brief Emitted when the selected tensor changes
     *
     * @param key The new tensor key (may be empty if cleared)
     */
    void tensorSelectionChanged(QString const & key);

    /**
     * @brief Emitted when selection validity changes
     *
     * @param valid Whether the current selection is valid for ML workflows
     */
    void validityChanged(bool valid);

private slots:
    void _onTensorComboChanged(int index);

private:
    void _setupConnections();
    void _updateTensorInfo();
    void _registerDataManagerObserver();

    Ui::FeatureSelectionPanel * ui;
    std::shared_ptr<MLCoreWidgetState> _state;
    std::shared_ptr<DataManager> _data_manager;
    int _dm_observer_id = -1;
    bool _valid = false;
};

#endif // FEATURE_SELECTION_PANEL_HPP
