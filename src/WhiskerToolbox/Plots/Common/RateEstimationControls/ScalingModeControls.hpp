#ifndef SCALING_MODE_CONTROLS_HPP
#define SCALING_MODE_CONTROLS_HPP

/**
 * @file ScalingModeControls.hpp
 * @brief Composable widget for selecting a rate-estimate scaling/normalization mode
 *
 * Provides a labeled combo box listing all `ScalingMode` values from
 * `RateEstimate.hpp`. Designed to be embedded in properties panels of
 * any widget that needs to choose how rate estimates are normalized
 * (e.g., HeatmapPropertiesWidget, PSTHPropertiesWidget).
 *
 * **Usage**:
 * @code
 *   auto * controls = new ScalingModeControls(parent);
 *   layout->addWidget(controls);
 *
 *   connect(controls, &ScalingModeControls::scalingModeChanged,
 *           this, [this](ScalingMode mode) {
 *               state->setScaling(mode);
 *           });
 * @endcode
 *
 * @see RateEstimate.hpp for ScalingMode enum and helper functions
 * @see RateNormalization.hpp for applyScaling()
 */

#include "Plots/Common/EventRateEstimation/RateEstimate.hpp"

#include <QWidget>

class QComboBox;

/**
 * @brief Widget for selecting a ScalingMode via combo box
 *
 * Compact widget: optional label + combo box populated with all
 * `ScalingMode` values. Emits `scalingModeChanged` when the user
 * selects a different mode.
 */
class ScalingModeControls : public QWidget {
    Q_OBJECT

public:
    /**
     * @brief Construct scaling mode controls
     * @param parent Parent widget
     * @param show_label Whether to show a "Scaling" label above the combo box
     */
    explicit ScalingModeControls(QWidget * parent = nullptr,
                                 bool show_label = true);
    ~ScalingModeControls() override = default;

    /**
     * @brief Get the currently selected scaling mode
     */
    [[nodiscard]] WhiskerToolbox::Plots::ScalingMode scalingMode() const;

    /**
     * @brief Set the scaling mode (updates combo box without emitting signal)
     */
    void setScalingMode(WhiskerToolbox::Plots::ScalingMode mode);

    /**
     * @brief Get the underlying combo box for custom styling or placement
     */
    [[nodiscard]] QComboBox * comboBox() const { return _combo; }

signals:
    /**
     * @brief Emitted when the user selects a different scaling mode
     * @param mode The newly selected ScalingMode
     */
    void scalingModeChanged(WhiskerToolbox::Plots::ScalingMode mode);

private slots:
    void onComboChanged(int index);

private:
    QComboBox * _combo = nullptr;
    bool _updating_ui = false;
};

#endif // SCALING_MODE_CONTROLS_HPP
