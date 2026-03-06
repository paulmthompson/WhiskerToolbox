#ifndef COLORMAP_CONTROLS_HPP
#define COLORMAP_CONTROLS_HPP

/**
 * @file ColormapControls.hpp
 * @brief Composable widget for selecting a colormap preset and configuring color range
 *
 * Provides a combo box for colormap preset selection (with painted gradient icons)
 * and a color range mode selector (Auto / Manual / Symmetric) with vmin/vmax
 * spinboxes. Designed to be embedded in any properties panel that renders
 * scalar-to-color mappings (e.g., HeatmapWidget, SpectrogramWidget).
 *
 * @see CorePlotting::Colormaps::ColormapPreset for available presets
 * @see CorePlotting::Colormaps::getColormap for obtaining a ColormapFunction
 */

#include "CorePlotting/Colormaps/Colormap.hpp"

#include <QWidget>

class QComboBox;
class QDoubleSpinBox;
class QLabel;

/**
 * @brief Configuration for how the displayed color range is determined
 */
struct ColorRangeConfig {
    enum class Mode {
        Auto,     ///< Range from data min/max (default)
        Manual,   ///< User-specified vmin/vmax
        Symmetric,///< Symmetric around zero (for z-scores)
    };

    Mode mode = Mode::Auto;
    double vmin = 0.0;///< Manual minimum (used when mode == Manual)
    double vmax = 1.0;///< Manual maximum (used when mode == Manual)
};

/**
 * @brief Widget for selecting a colormap preset and configuring color range
 *
 * Presents two logical groups:
 * 1. **Colormap selection** — combo box populated with all `ColormapPreset`
 *    values, each accompanied by a small painted gradient icon.
 * 2. **Color range** — mode combo (Auto / Manual / Symmetric) with
 *    vmin/vmax `QDoubleSpinBox` widgets visible only in Manual mode.
 *
 * Emits `colormapChanged` when the user picks a different preset, and
 * `colorRangeChanged` when the range mode or bounds change.
 */
class ColormapControls : public QWidget {
    Q_OBJECT

public:
    explicit ColormapControls(QWidget * parent = nullptr);
    ~ColormapControls() override = default;

    // --- Colormap preset ---

    /** @brief Get the currently selected colormap preset */
    [[nodiscard]] CorePlotting::Colormaps::ColormapPreset colormapPreset() const;

    /** @brief Set the colormap preset (updates combo without emitting signal) */
    void setColormapPreset(CorePlotting::Colormaps::ColormapPreset preset);

    // --- Color range ---

    /** @brief Get the currently configured color range */
    [[nodiscard]] ColorRangeConfig colorRange() const;

    /** @brief Set the full color range config (updates UI without emitting signal) */
    void setColorRange(ColorRangeConfig const & config);

    /** @brief Set just the color range mode */
    void setColorRangeMode(ColorRangeConfig::Mode mode);

    /** @brief Set the manual vmin/vmax bounds */
    void setColorRangeBounds(double vmin, double vmax);

signals:
    /** @brief Emitted when the user selects a different colormap preset */
    void colormapChanged(CorePlotting::Colormaps::ColormapPreset preset);

    /** @brief Emitted when the color range mode or manual bounds change */
    void colorRangeChanged(ColorRangeConfig const & config);

private:
    void updateColorRangeVisibility();
    [[nodiscard]] static QIcon createColormapIcon(CorePlotting::Colormaps::ColormapPreset preset);

    QComboBox * _colormap_combo = nullptr;
    QComboBox * _range_mode_combo = nullptr;
    QDoubleSpinBox * _vmin_spin = nullptr;
    QDoubleSpinBox * _vmax_spin = nullptr;
    QLabel * _vmin_label = nullptr;
    QLabel * _vmax_label = nullptr;

    bool _updating_ui = false;///< Anti-recursion guard for programmatic updates
};

#endif// COLORMAP_CONTROLS_HPP
