#include "ColormapControls.hpp"

#include "CorePlotting/Colormaps/Colormap.hpp"

#include <QComboBox>
#include <QDoubleSpinBox>
#include <QFormLayout>
#include <QIcon>
#include <QLabel>
#include <QLinearGradient>
#include <QPainter>
#include <QPixmap>
#include <QVBoxLayout>

using namespace CorePlotting::Colormaps;

// =============================================================================
// Construction
// =============================================================================

ColormapControls::ColormapControls(QWidget * parent)
    : QWidget(parent) {
    auto * top_layout = new QVBoxLayout(this);
    top_layout->setContentsMargins(0, 0, 0, 0);
    top_layout->setSpacing(4);

    // --- Colormap preset combo box ---
    auto * colormap_label = new QLabel(tr("Colormap"), this);
    colormap_label->setStyleSheet("font-weight: bold;");
    top_layout->addWidget(colormap_label);

    _colormap_combo = new QComboBox(this);
    _colormap_combo->setObjectName("colormap_preset_combo");
    for (auto const preset: allPresets()) {
        _colormap_combo->addItem(
                createColormapIcon(preset),
                QString::fromStdString(presetName(preset)),
                static_cast<int>(preset));
    }
    _colormap_combo->setIconSize(QSize(64, 16));
    top_layout->addWidget(_colormap_combo);

    // --- Color range section ---
    auto * range_form = new QFormLayout();
    range_form->setContentsMargins(0, 8, 0, 0);
    range_form->setSpacing(4);

    _range_mode_combo = new QComboBox(this);
    _range_mode_combo->setObjectName("color_range_mode_combo");
    _range_mode_combo->addItem("Auto", static_cast<int>(ColorRangeConfig::Mode::Auto));
    _range_mode_combo->addItem("Manual", static_cast<int>(ColorRangeConfig::Mode::Manual));
    _range_mode_combo->addItem("Symmetric", static_cast<int>(ColorRangeConfig::Mode::Symmetric));
    range_form->addRow("Color Range:", _range_mode_combo);

    _vmin_label = new QLabel("Min", this);
    _vmin_spin = new QDoubleSpinBox(this);
    _vmin_spin->setObjectName("vmin_spin");
    _vmin_spin->setRange(-1e9, 1e9);
    _vmin_spin->setDecimals(4);
    _vmin_spin->setSingleStep(0.1);
    _vmin_spin->setValue(0.0);
    range_form->addRow(_vmin_label, _vmin_spin);

    _vmax_label = new QLabel("Max", this);
    _vmax_spin = new QDoubleSpinBox(this);
    _vmax_spin->setObjectName("vmax_spin");
    _vmax_spin->setRange(-1e9, 1e9);
    _vmax_spin->setDecimals(4);
    _vmax_spin->setSingleStep(0.1);
    _vmax_spin->setValue(1.0);
    range_form->addRow(_vmax_label, _vmax_spin);

    top_layout->addLayout(range_form);

    // Initial visibility
    updateColorRangeVisibility();

    // --- Connect signals ---
    connect(_colormap_combo, &QComboBox::currentIndexChanged,
            this, [this](int index) {
                if (_updating_ui || index < 0) return;
                auto preset = static_cast<ColormapPreset>(
                        _colormap_combo->itemData(index).toInt());
                emit colormapChanged(preset);
            });

    connect(_range_mode_combo, &QComboBox::currentIndexChanged,
            this, [this](int index) {
                if (_updating_ui || index < 0) return;
                updateColorRangeVisibility();
                emit colorRangeChanged(colorRange());
            });

    connect(_vmin_spin, &QDoubleSpinBox::valueChanged,
            this, [this]([[maybe_unused]] double val) {
                if (_updating_ui) return;
                emit colorRangeChanged(colorRange());
            });

    connect(_vmax_spin, &QDoubleSpinBox::valueChanged,
            this, [this]([[maybe_unused]] double val) {
                if (_updating_ui) return;
                emit colorRangeChanged(colorRange());
            });
}

// =============================================================================
// Colormap Preset
// =============================================================================

ColormapPreset ColormapControls::colormapPreset() const {
    return static_cast<ColormapPreset>(_colormap_combo->currentData().toInt());
}

void ColormapControls::setColormapPreset(ColormapPreset preset) {
    _updating_ui = true;
    int const idx = _colormap_combo->findData(static_cast<int>(preset));
    if (idx >= 0) {
        _colormap_combo->setCurrentIndex(idx);
    }
    _updating_ui = false;
}

// =============================================================================
// Color Range
// =============================================================================

ColorRangeConfig ColormapControls::colorRange() const {
    ColorRangeConfig config;
    config.mode = static_cast<ColorRangeConfig::Mode>(
            _range_mode_combo->currentData().toInt());
    config.vmin = _vmin_spin->value();
    config.vmax = _vmax_spin->value();
    return config;
}

void ColormapControls::setColorRange(ColorRangeConfig const & config) {
    _updating_ui = true;

    int const mode_idx = _range_mode_combo->findData(static_cast<int>(config.mode));
    if (mode_idx >= 0) {
        _range_mode_combo->setCurrentIndex(mode_idx);
    }
    _vmin_spin->setValue(config.vmin);
    _vmax_spin->setValue(config.vmax);

    updateColorRangeVisibility();
    _updating_ui = false;
}

void ColormapControls::setColorRangeMode(ColorRangeConfig::Mode mode) {
    _updating_ui = true;
    int const idx = _range_mode_combo->findData(static_cast<int>(mode));
    if (idx >= 0) {
        _range_mode_combo->setCurrentIndex(idx);
    }
    updateColorRangeVisibility();
    _updating_ui = false;
}

void ColormapControls::setColorRangeBounds(double vmin, double vmax) {
    _updating_ui = true;
    _vmin_spin->setValue(vmin);
    _vmax_spin->setValue(vmax);
    _updating_ui = false;
}

// =============================================================================
// Private Helpers
// =============================================================================

void ColormapControls::updateColorRangeVisibility() {
    bool const is_manual = static_cast<ColorRangeConfig::Mode>(
                                   _range_mode_combo->currentData().toInt()) ==
                           ColorRangeConfig::Mode::Manual;
    _vmin_label->setVisible(is_manual);
    _vmin_spin->setVisible(is_manual);
    _vmax_label->setVisible(is_manual);
    _vmax_spin->setVisible(is_manual);
}

QIcon ColormapControls::createColormapIcon(ColormapPreset preset) {
    constexpr int width = 64;
    constexpr int height = 16;

    QPixmap pixmap(width, height);
    pixmap.fill(Qt::transparent);

    QPainter painter(&pixmap);
    painter.setRenderHint(QPainter::Antialiasing, false);

    auto const & lut = getLUT(preset);

    // Draw a horizontal gradient strip using the LUT entries
    for (int x = 0; x < width; ++x) {
        float const t = static_cast<float>(x) / static_cast<float>(width - 1);
        auto const idx = static_cast<std::size_t>(t * 255.0f);
        auto const & c = lut[std::min(idx, std::size_t{255})];
        QColor const color(
                static_cast<int>(c.r * 255.0f),
                static_cast<int>(c.g * 255.0f),
                static_cast<int>(c.b * 255.0f));
        painter.setPen(color);
        painter.drawLine(x, 0, x, height - 1);
    }

    painter.end();
    return {pixmap};
}
