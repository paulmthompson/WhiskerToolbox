#ifndef PREDICTION_PANEL_HPP
#define PREDICTION_PANEL_HPP

/**
 * @file PredictionPanel.hpp
 * @brief Panel for configuring and running ML predictions
 *
 * PredictionPanel provides the prediction configuration UI for the
 * Classification tab in MLCoreWidget. It contains:
 *
 * 1. **Prediction region selector** — combo box listing DigitalIntervalSeries
 *    keys in the DataManager, with an "All remaining frames" checkbox to
 *    predict on every frame not used for training.
 *
 * 2. **Output prefix** — text field for naming generated output data keys
 *    (e.g. "ml" → "Predicted:ml_class0", "ml_prob_class0").
 *
 * 3. **Threshold slider/spin box** — decision threshold for binary
 *    classification probability (0.01–0.99, default 0.50). The slider and
 *    spin box are bidirectionally synchronized.
 *
 * 4. **Output type checkboxes**:
 *    - Output predictions as DigitalIntervalSeries (class intervals)
 *    - Output probabilities as AnalogTimeSeries (per-class probabilities)
 *
 * 5. **Predict button** — emits `predictRequested()` for the parent widget
 *    to wire up to the ClassificationPipeline.
 *
 * ## State Integration
 *
 * The panel reads/writes these MLCoreWidgetState fields:
 * - `prediction_region_key` — DigitalIntervalSeries key for the prediction region
 * - `output_prefix` — prefix for generated data keys
 * - `probability_threshold` — decision threshold (0.0–1.0)
 * - `output_probabilities` — whether to generate probability AnalogTimeSeries
 * - `output_predictions` — whether to generate prediction DigitalIntervalSeries
 *
 * ## Integration
 *
 * - Embedded in the Classification tab of MLCoreWidget below ModelConfigPanel
 * - Refreshes automatically when DataManager contents change
 * - Validates that a prediction region is configured and at least one output
 *   type is selected
 *
 * @see MLCoreWidget for the parent widget
 * @see MLCoreWidgetState for the persisted prediction configuration
 * @see ClassificationPipeline for the pipeline this panel triggers
 * @see PredictionWriter for how predictions are written to DataManager
 */

#include <QWidget>

#include <memory>
#include <string>

class DataManager;
class MLCoreWidgetState;

namespace Ui {
class PredictionPanel;
}

class PredictionPanel : public QWidget {
    Q_OBJECT

public:
    /**
     * @brief Construct the prediction panel
     *
     * @param state Shared MLCoreWidgetState for reading/writing prediction settings
     * @param data_manager Shared DataManager for listing DigitalIntervalSeries keys
     * @param parent Optional parent widget
     */
    explicit PredictionPanel(std::shared_ptr<MLCoreWidgetState> state,
                             std::shared_ptr<DataManager> data_manager,
                             QWidget * parent = nullptr);

    ~PredictionPanel() override;

    /**
     * @brief Whether a valid prediction configuration is ready
     *
     * Returns true when a prediction region is configured (or "All remaining
     * frames" is checked) and at least one output type is selected.
     */
    [[nodiscard]] bool hasValidConfiguration() const;

    /**
     * @brief Get the currently selected prediction region key
     *
     * @return The DataManager key of the selected DigitalIntervalSeries,
     *         or empty string if "All remaining frames" is checked
     */
    [[nodiscard]] std::string selectedRegionKey() const;

    /**
     * @brief Whether the "All remaining frames" checkbox is checked
     */
    [[nodiscard]] bool isAllFramesChecked() const;

    /**
     * @brief Get the current decision threshold
     */
    [[nodiscard]] double threshold() const;

    /**
     * @brief Get the output prefix string
     */
    [[nodiscard]] std::string outputPrefix() const;

    /**
     * @brief Whether prediction output (DigitalIntervalSeries) is enabled
     */
    [[nodiscard]] bool outputPredictions() const;

    /**
     * @brief Whether probability output (AnalogTimeSeries) is enabled
     */
    [[nodiscard]] bool outputProbabilities() const;

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
     * @brief Emitted when the user clicks the Predict button
     *
     * The parent widget should connect this to trigger prediction via
     * the trained model on the configured region.
     */
    void predictRequested();

    /**
     * @brief Emitted when the selected prediction region changes
     *
     * @param key The new region key (empty if "All remaining frames")
     */
    void regionChanged(QString const & key);

    /**
     * @brief Emitted when the decision threshold changes
     *
     * @param value The new threshold value (0.01–0.99)
     */
    void thresholdChanged(double value);

    /**
     * @brief Emitted when any output configuration changes
     */
    void outputConfigChanged();

    /**
     * @brief Emitted when the overall validity of the configuration changes
     *
     * @param valid Whether the current configuration is valid for prediction
     */
    void validityChanged(bool valid);

private slots:
    void _onRegionComboChanged(int index);
    void _onAllFramesToggled(bool checked);
    void _onThresholdSpinBoxChanged(double value);
    void _onThresholdSliderChanged(int value);
    void _onOutputPrefixChanged(QString const & text);
    void _onOutputPredictionsToggled(bool checked);
    void _onOutputProbabilitiesToggled(bool checked);
    void _onPredictClicked();

private:
    void _setupConnections();
    void _registerDataManagerObserver();
    void _updateRegionInfo();
    void _updateValidity();
    void _restoreFromState();
    void _syncRegionToState(std::string const & key);

    Ui::PredictionPanel * ui;
    std::shared_ptr<MLCoreWidgetState> _state;
    std::shared_ptr<DataManager> _data_manager;
    int _dm_observer_id = -1;
    bool _valid = false;
    bool _updating = false;///< Suppress state writes during programmatic updates
};

#endif// PREDICTION_PANEL_HPP
