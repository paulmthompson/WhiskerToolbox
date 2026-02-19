#ifndef REGION_SELECTION_PANEL_HPP
#define REGION_SELECTION_PANEL_HPP

/**
 * @file RegionSelectionPanel.hpp
 * @brief Panel for selecting a DigitalIntervalSeries region in the ML workflow
 *
 * RegionSelectionPanel displays a combo box listing all DigitalIntervalSeries
 * keys in the DataManager, along with summary information about the selected
 * interval series:
 *
 * - **Interval count** — number of intervals in the series
 * - **Total frame count** — sum of frames across all intervals
 * - **Time frame** name if the series has an associated TimeFrame
 *
 * An "All frames" checkbox allows the user to bypass region selection and use
 * all available frames. When checked, the combo box is disabled and the
 * state key is set to empty.
 *
 * The panel operates in one of two modes (`RegionMode`) to bind to the
 * appropriate state field:
 *
 * - **Training**: reads/writes `training_region_key` in MLCoreWidgetState
 * - **Prediction**: reads/writes `prediction_region_key` in MLCoreWidgetState
 *
 * ## Integration
 *
 * - Embedded in the Classification tab of MLCoreWidget (for training region)
 * - Can be reused in PredictionPanel (task 4.6) for prediction region
 * - Refreshes automatically when DataManager contents change
 * - Validates that a region is selected or "All frames" is checked
 *
 * @see MLCoreWidget for the parent widget
 * @see MLCoreWidgetState::trainingRegionKey for the training region state
 * @see MLCoreWidgetState::predictionRegionKey for the prediction region state
 * @see DigitalIntervalSeries for the data type being selected
 */

#include <QWidget>

#include <memory>
#include <string>

// Forward declarations
class DataManager;
class MLCoreWidgetState;

namespace Ui {
class RegionSelectionPanel;
}

/**
 * @brief Determines which state field the panel binds to
 */
enum class RegionMode {
    Training,  ///< Binds to MLCoreWidgetState::trainingRegionKey
    Prediction ///< Binds to MLCoreWidgetState::predictionRegionKey
};

class RegionSelectionPanel : public QWidget {
    Q_OBJECT

public:
    /**
     * @brief Construct the region selection panel
     *
     * @param state Shared MLCoreWidgetState for reading/writing the region key
     * @param data_manager Shared DataManager for listing DigitalIntervalSeries keys
     * @param mode Whether this panel manages the training or prediction region
     * @param parent Optional parent widget
     */
    explicit RegionSelectionPanel(std::shared_ptr<MLCoreWidgetState> state,
                                  std::shared_ptr<DataManager> data_manager,
                                  RegionMode mode,
                                  QWidget * parent = nullptr);

    ~RegionSelectionPanel() override;

    /**
     * @brief Whether a valid region is currently configured
     *
     * Returns true if "All frames" is checked, or if the combo box has a
     * non-empty selection that resolves to a non-null DigitalIntervalSeries
     * with at least one interval.
     */
    [[nodiscard]] bool hasValidSelection() const;

    /**
     * @brief Get the currently selected region key
     *
     * @return The DataManager key of the selected DigitalIntervalSeries,
     *         or empty string if "All frames" is checked or nothing is selected
     */
    [[nodiscard]] std::string selectedRegionKey() const;

    /**
     * @brief Whether the "All frames" checkbox is checked
     */
    [[nodiscard]] bool isAllFramesChecked() const;

public slots:
    /**
     * @brief Refresh the list of available DigitalIntervalSeries keys
     *
     * Called automatically on DataManager changes and on the refresh button.
     * Preserves the current selection if still available.
     */
    void refreshRegionList();

signals:
    /**
     * @brief Emitted when the selected region changes
     *
     * @param key The new region key (empty if "All frames" or cleared)
     */
    void regionSelectionChanged(QString const & key);

    /**
     * @brief Emitted when selection validity changes
     *
     * @param valid Whether the current configuration is valid for ML workflows
     */
    void validityChanged(bool valid);

    /**
     * @brief Emitted when the "All frames" checkbox state changes
     *
     * @param all_frames Whether "All frames" is now checked
     */
    void allFramesToggled(bool all_frames);

private slots:
    void _onRegionComboChanged(int index);
    void _onAllFramesToggled(bool checked);

private:
    void _setupConnections();
    void _updateRegionInfo();
    void _registerDataManagerObserver();
    void _syncToState(std::string const & key);
    [[nodiscard]] std::string _readKeyFromState() const;

    Ui::RegionSelectionPanel * ui;
    std::shared_ptr<MLCoreWidgetState> _state;
    std::shared_ptr<DataManager> _data_manager;
    RegionMode _mode;
    int _dm_observer_id = -1;
    bool _valid = false;
};

#endif // REGION_SELECTION_PANEL_HPP
