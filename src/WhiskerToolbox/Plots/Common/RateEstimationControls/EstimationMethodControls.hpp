#ifndef ESTIMATION_METHOD_CONTROLS_HPP
#define ESTIMATION_METHOD_CONTROLS_HPP

/**
 * @file EstimationMethodControls.hpp
 * @brief Composable widget for selecting a rate estimation method and its parameters
 *
 * Provides a combo box listing all supported estimation methods (BinningParams,
 * GaussianKernelParams, CausalExponentialParams). When the user selects a method
 * from the combo box, a method-specific parameter panel is shown below it, with
 * appropriate spin boxes for that method's tuneable parameters.
 *
 * This widget is designed to be embedded in any properties panel that needs
 * estimation method selection (e.g., HeatmapPropertiesWidget, PSTHPropertiesWidget).
 *
 * **Usage**:
 * @code
 *   auto * controls = new EstimationMethodControls(parent);
 *   layout->addWidget(controls);
 *
 *   connect(controls, &EstimationMethodControls::paramsChanged,
 *           this, [this](auto const & params) {
 *               // Use params with estimateRate() / estimateRates()
 *           });
 * @endcode
 *
 * @see EventRateEstimation.hpp for EstimationParams and the per-method structs
 */

#include "Plots/Common/EventRateEstimation/EventRateEstimation.hpp"

#include <QWidget>

class QComboBox;
class QDoubleSpinBox;
class QLabel;
class QStackedWidget;

/**
 * @brief Widget for selecting a rate estimation method and configuring its parameters
 *
 * Shows a combo box for method selection and a stacked widget below it
 * with method-specific parameter controls. Unimplemented methods (Gaussian,
 * Causal Exponential) have their parameter controls disabled.
 */
class EstimationMethodControls : public QWidget {
    Q_OBJECT

public:
    explicit EstimationMethodControls(QWidget * parent = nullptr);
    ~EstimationMethodControls() override = default;

    /**
     * @brief Get the currently configured estimation parameters
     * @return EstimationParams variant matching the selected method
     */
    [[nodiscard]] WhiskerToolbox::Plots::EstimationParams params() const;

    /**
     * @brief Set the estimation parameters (updates combo box and parameter controls)
     * @param params EstimationParams variant to apply
     */
    void setParams(WhiskerToolbox::Plots::EstimationParams const & params);

signals:
    /**
     * @brief Emitted when the user changes the estimation method or any parameter
     * @param params The new EstimationParams variant
     */
    void paramsChanged(WhiskerToolbox::Plots::EstimationParams const & params);

private slots:
    void onMethodChanged(int index);
    void onBinSizeChanged(double value);
    void onGaussianSigmaChanged(double value);
    void onGaussianEvalStepChanged(double value);
    void onCausalTauChanged(double value);
    void onCausalEvalStepChanged(double value);

private:
    void emitCurrentParams();

    QComboBox * _method_combo = nullptr;
    QStackedWidget * _params_stack = nullptr;

    // --- Binning page ---
    QDoubleSpinBox * _bin_size_spin = nullptr;

    // --- Gaussian Kernel page ---
    QDoubleSpinBox * _gaussian_sigma_spin = nullptr;
    QDoubleSpinBox * _gaussian_eval_step_spin = nullptr;

    // --- Causal Exponential page ---
    QDoubleSpinBox * _causal_tau_spin = nullptr;
    QDoubleSpinBox * _causal_eval_step_spin = nullptr;

    bool _updating_ui = false; ///< Anti-recursion guard for programmatic updates
};

#endif // ESTIMATION_METHOD_CONTROLS_HPP
