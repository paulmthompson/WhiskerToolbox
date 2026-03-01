#include "ScalingModeControls.hpp"

#include <QComboBox>
#include <QLabel>
#include <QVBoxLayout>

using namespace WhiskerToolbox::Plots;

// =============================================================================
// Construction
// =============================================================================

ScalingModeControls::ScalingModeControls(QWidget * parent, bool show_label)
    : QWidget(parent)
{
    auto * layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(4);

    if (show_label) {
        auto * label = new QLabel(tr("Scaling"), this);
        label->setStyleSheet("font-weight: bold;");
        layout->addWidget(label);
    }

    _combo = new QComboBox(this);
    _combo->setObjectName("scaling_mode_combo");

    for (auto mode : allScalingModes()) {
        _combo->addItem(
            QString::fromUtf8(scalingLabel(mode)),
            static_cast<int>(mode));
    }
    layout->addWidget(_combo);

    connect(_combo, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &ScalingModeControls::onComboChanged);
}

// =============================================================================
// Public API
// =============================================================================

ScalingMode ScalingModeControls::scalingMode() const
{
    return static_cast<ScalingMode>(_combo->currentData().toInt());
}

void ScalingModeControls::setScalingMode(ScalingMode mode)
{
    _updating_ui = true;
    int const target = static_cast<int>(mode);
    for (int i = 0; i < _combo->count(); ++i) {
        if (_combo->itemData(i).toInt() == target) {
            _combo->setCurrentIndex(i);
            break;
        }
    }
    _updating_ui = false;
}

// =============================================================================
// Private slots
// =============================================================================

void ScalingModeControls::onComboChanged(int /*index*/)
{
    if (!_updating_ui) {
        emit scalingModeChanged(scalingMode());
    }
}
