#include "EstimationMethodControls.hpp"

#include <QComboBox>
#include <QDoubleSpinBox>
#include <QFormLayout>
#include <QLabel>
#include <QStackedWidget>
#include <QVBoxLayout>

#include <variant>

using namespace WhiskerToolbox::Plots;

// =============================================================================
// Method index constants (must match insertion order in constructor)
// =============================================================================

namespace {
constexpr int kBinningIndex = 0;
constexpr int kGaussianIndex = 1;
constexpr int kCausalExpIndex = 2;
} // namespace

// =============================================================================
// Construction
// =============================================================================

EstimationMethodControls::EstimationMethodControls(QWidget * parent)
    : QWidget(parent)
{
    auto * top_layout = new QVBoxLayout(this);
    top_layout->setContentsMargins(0, 0, 0, 0);
    top_layout->setSpacing(4);

    // --- Method combo box ---
    auto * method_label = new QLabel(tr("Estimation Method"), this);
    method_label->setStyleSheet("font-weight: bold;");
    top_layout->addWidget(method_label);

    _method_combo = new QComboBox(this);
    _method_combo->setObjectName("estimation_method_combo");
    _method_combo->addItem(tr("Histogram Binning"));
    _method_combo->addItem(tr("Gaussian Kernel (stub)"));
    _method_combo->addItem(tr("Causal Exponential (stub)"));
    top_layout->addWidget(_method_combo);

    // --- Stacked widget for per-method parameters ---
    _params_stack = new QStackedWidget(this);
    top_layout->addWidget(_params_stack);

    // ---- Page 0: Binning ----
    {
        auto * page = new QWidget(_params_stack);
        auto * form = new QFormLayout(page);
        form->setContentsMargins(0, 4, 0, 0);

        _bin_size_spin = new QDoubleSpinBox(page);
        _bin_size_spin->setObjectName("bin_size_spin");
        _bin_size_spin->setRange(0.1, 10000.0);
        _bin_size_spin->setDecimals(1);
        _bin_size_spin->setSingleStep(1.0);
        _bin_size_spin->setValue(BinningParams{}.bin_size);
        _bin_size_spin->setSuffix(tr(" time units"));
        form->addRow(tr("Bin Size"), _bin_size_spin);

        _params_stack->addWidget(page); // index 0
    }

    // ---- Page 1: Gaussian Kernel (stub — disabled) ----
    {
        auto * page = new QWidget(_params_stack);
        auto * form = new QFormLayout(page);
        form->setContentsMargins(0, 4, 0, 0);

        _gaussian_sigma_spin = new QDoubleSpinBox(page);
        _gaussian_sigma_spin->setObjectName("gaussian_sigma_spin");
        _gaussian_sigma_spin->setRange(0.1, 10000.0);
        _gaussian_sigma_spin->setDecimals(1);
        _gaussian_sigma_spin->setSingleStep(1.0);
        _gaussian_sigma_spin->setValue(GaussianKernelParams{}.sigma);
        _gaussian_sigma_spin->setSuffix(tr(" time units"));
        _gaussian_sigma_spin->setEnabled(false);
        form->addRow(tr("Sigma (σ)"), _gaussian_sigma_spin);

        _gaussian_eval_step_spin = new QDoubleSpinBox(page);
        _gaussian_eval_step_spin->setObjectName("gaussian_eval_step_spin");
        _gaussian_eval_step_spin->setRange(0.01, 1000.0);
        _gaussian_eval_step_spin->setDecimals(2);
        _gaussian_eval_step_spin->setSingleStep(0.5);
        _gaussian_eval_step_spin->setValue(GaussianKernelParams{}.eval_step);
        _gaussian_eval_step_spin->setSuffix(tr(" time units"));
        _gaussian_eval_step_spin->setEnabled(false);
        form->addRow(tr("Eval Step"), _gaussian_eval_step_spin);

        auto * stub_label = new QLabel(tr("Not yet implemented"), page);
        stub_label->setStyleSheet("color: gray; font-style: italic;");
        form->addRow(stub_label);

        _params_stack->addWidget(page); // index 1
    }

    // ---- Page 2: Causal Exponential (stub — disabled) ----
    {
        auto * page = new QWidget(_params_stack);
        auto * form = new QFormLayout(page);
        form->setContentsMargins(0, 4, 0, 0);

        _causal_tau_spin = new QDoubleSpinBox(page);
        _causal_tau_spin->setObjectName("causal_tau_spin");
        _causal_tau_spin->setRange(0.1, 10000.0);
        _causal_tau_spin->setDecimals(1);
        _causal_tau_spin->setSingleStep(1.0);
        _causal_tau_spin->setValue(CausalExponentialParams{}.tau);
        _causal_tau_spin->setSuffix(tr(" time units"));
        _causal_tau_spin->setEnabled(false);
        form->addRow(tr("Tau (τ)"), _causal_tau_spin);

        _causal_eval_step_spin = new QDoubleSpinBox(page);
        _causal_eval_step_spin->setObjectName("causal_eval_step_spin");
        _causal_eval_step_spin->setRange(0.01, 1000.0);
        _causal_eval_step_spin->setDecimals(2);
        _causal_eval_step_spin->setSingleStep(0.5);
        _causal_eval_step_spin->setValue(CausalExponentialParams{}.eval_step);
        _causal_eval_step_spin->setSuffix(tr(" time units"));
        _causal_eval_step_spin->setEnabled(false);
        form->addRow(tr("Eval Step"), _causal_eval_step_spin);

        auto * stub_label = new QLabel(tr("Not yet implemented"), page);
        stub_label->setStyleSheet("color: gray; font-style: italic;");
        form->addRow(stub_label);

        _params_stack->addWidget(page); // index 2
    }

    // --- Connections ---
    connect(_method_combo, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &EstimationMethodControls::onMethodChanged);
    connect(_bin_size_spin, QOverload<double>::of(&QDoubleSpinBox::valueChanged),
            this, &EstimationMethodControls::onBinSizeChanged);
    connect(_gaussian_sigma_spin, QOverload<double>::of(&QDoubleSpinBox::valueChanged),
            this, &EstimationMethodControls::onGaussianSigmaChanged);
    connect(_gaussian_eval_step_spin, QOverload<double>::of(&QDoubleSpinBox::valueChanged),
            this, &EstimationMethodControls::onGaussianEvalStepChanged);
    connect(_causal_tau_spin, QOverload<double>::of(&QDoubleSpinBox::valueChanged),
            this, &EstimationMethodControls::onCausalTauChanged);
    connect(_causal_eval_step_spin, QOverload<double>::of(&QDoubleSpinBox::valueChanged),
            this, &EstimationMethodControls::onCausalEvalStepChanged);

    // Default: binning
    _method_combo->setCurrentIndex(kBinningIndex);
    _params_stack->setCurrentIndex(kBinningIndex);
}

// =============================================================================
// Public API
// =============================================================================

EstimationParams EstimationMethodControls::params() const
{
    switch (_method_combo->currentIndex()) {
        case kBinningIndex:
            return BinningParams{.bin_size = _bin_size_spin->value()};
        case kGaussianIndex:
            return GaussianKernelParams{
                .sigma = _gaussian_sigma_spin->value(),
                .eval_step = _gaussian_eval_step_spin->value()};
        case kCausalExpIndex:
            return CausalExponentialParams{
                .tau = _causal_tau_spin->value(),
                .eval_step = _causal_eval_step_spin->value()};
        default:
            return BinningParams{};
    }
}

void EstimationMethodControls::setParams(EstimationParams const & params)
{
    _updating_ui = true;

    std::visit([this](auto const & p) {
        using T = std::decay_t<decltype(p)>;
        if constexpr (std::is_same_v<T, BinningParams>) {
            _method_combo->setCurrentIndex(kBinningIndex);
            _params_stack->setCurrentIndex(kBinningIndex);
            _bin_size_spin->setValue(p.bin_size);
        } else if constexpr (std::is_same_v<T, GaussianKernelParams>) {
            _method_combo->setCurrentIndex(kGaussianIndex);
            _params_stack->setCurrentIndex(kGaussianIndex);
            _gaussian_sigma_spin->setValue(p.sigma);
            _gaussian_eval_step_spin->setValue(p.eval_step);
        } else if constexpr (std::is_same_v<T, CausalExponentialParams>) {
            _method_combo->setCurrentIndex(kCausalExpIndex);
            _params_stack->setCurrentIndex(kCausalExpIndex);
            _causal_tau_spin->setValue(p.tau);
            _causal_eval_step_spin->setValue(p.eval_step);
        }
    }, params);

    _updating_ui = false;
}

// =============================================================================
// Private slots
// =============================================================================

void EstimationMethodControls::onMethodChanged(int index)
{
    _params_stack->setCurrentIndex(index);
    if (!_updating_ui) {
        emitCurrentParams();
    }
}

void EstimationMethodControls::onBinSizeChanged(double /*value*/)
{
    if (!_updating_ui) { emitCurrentParams(); }
}

void EstimationMethodControls::onGaussianSigmaChanged(double /*value*/)
{
    if (!_updating_ui) { emitCurrentParams(); }
}

void EstimationMethodControls::onGaussianEvalStepChanged(double /*value*/)
{
    if (!_updating_ui) { emitCurrentParams(); }
}

void EstimationMethodControls::onCausalTauChanged(double /*value*/)
{
    if (!_updating_ui) { emitCurrentParams(); }
}

void EstimationMethodControls::onCausalEvalStepChanged(double /*value*/)
{
    if (!_updating_ui) { emitCurrentParams(); }
}

// =============================================================================
// Private helpers
// =============================================================================

void EstimationMethodControls::emitCurrentParams()
{
    emit paramsChanged(params());
}
