#ifndef SCATTER_PLOT_STATE_HPP
#define SCATTER_PLOT_STATE_HPP

/**
 * @file ScatterPlotState.hpp
 * @brief State class for ScatterPlotWidget
 *
 * ScatterPlotState manages the serializable state for the ScatterPlotWidget,
 * with a single source of truth for view state (zoom/pan) and axis ranges.
 * HorizontalAxisState and VerticalAxisState hold full axis ranges; view state
 * holds zoom/pan. Both axes are analog value axes (no time).
 *
 * @see EditorState for base class documentation
 * @see PSTHState, LinePlotState for the same pattern
 */

#include "ScatterAxisSource.hpp"
#include "CorePlotting/CoordinateTransform/ViewStateData.hpp"
#include "CorePlotting/DataTypes/GlyphStyleData.hpp"
#include "EditorState/EditorState.hpp"
#include "Plots/Common/GlyphStyleWidget/Core/GlyphStyleState.hpp"
#include "Plots/Common/HorizontalAxisWidget/Core/HorizontalAxisStateData.hpp"
#include "Plots/Common/HorizontalAxisWidget/Core/HorizontalAxisState.hpp"
#include "Plots/Common/VerticalAxisWidget/Core/VerticalAxisStateData.hpp"
#include "Plots/Common/VerticalAxisWidget/Core/VerticalAxisState.hpp"

#include <rfl.hpp>
#include <rfl/json.hpp>

#include <memory>
#include <optional>
#include <string>

/**
 * @brief Serializable state data for ScatterPlotWidget
 */
struct ScatterPlotStateData {
    std::string instance_id;
    std::string display_name = "Scatter Plot";
    CorePlotting::ViewStateData view_state;
    HorizontalAxisStateData horizontal_axis;
    VerticalAxisStateData vertical_axis;

    std::optional<ScatterAxisSource> x_source;  ///< X-axis data source
    std::optional<ScatterAxisSource> y_source;  ///< Y-axis data source
    bool show_reference_line = false;           ///< Show y=x reference line

    /// Glyph style for scatter points (shape, size, color, alpha)
    CorePlotting::GlyphStyleData glyph_style{CorePlotting::GlyphType::Circle, 5.0f, "#3388FF", 0.8f};
};

/**
 * @brief State class for ScatterPlotWidget
 *
 * Single source of truth: view_state (zoom/pan) plus horizontal and vertical
 * axis states (full range). OpenGL widget and axis widgets read from state.
 */
class ScatterPlotState : public EditorState {
    Q_OBJECT

public:
    explicit ScatterPlotState(QObject * parent = nullptr);
    ~ScatterPlotState() override = default;

    [[nodiscard]] QString getTypeName() const override { return QStringLiteral("ScatterPlotWidget"); }
    [[nodiscard]] QString getDisplayName() const override;
    void setDisplayName(QString const & name) override;

    // === Axis state access (for widgets and serialization) ===
    [[nodiscard]] HorizontalAxisState * horizontalAxisState() { return _horizontal_axis_state.get(); }
    [[nodiscard]] VerticalAxisState * verticalAxisState() { return _vertical_axis_state.get(); }

    // === Legacy accessors (delegate to axis states) ===
    [[nodiscard]] double getXMin() const;
    [[nodiscard]] double getXMax() const;
    [[nodiscard]] double getYMin() const;
    [[nodiscard]] double getYMax() const;

    // === View state (zoom / pan / bounds) ===
    /** @brief Get the current view state (zoom, pan, data bounds). */
    [[nodiscard]] CorePlotting::ViewStateData const & viewState() const { return _data.view_state; }
    void setXZoom(double zoom);
    void setYZoom(double zoom);
    void setPan(double x_pan, double y_pan);
    /** @brief Set X data bounds; updates view state and horizontal axis. */
    void setXBounds(double x_min, double x_max);
    /** @brief Set Y data bounds; updates view state and vertical axis. */
    void setYBounds(double y_min, double y_max);

    // === Data source configuration ===
    [[nodiscard]] std::optional<ScatterAxisSource> const & xSource() const { return _data.x_source; }
    [[nodiscard]] std::optional<ScatterAxisSource> const & ySource() const { return _data.y_source; }
    void setXSource(std::optional<ScatterAxisSource> source);
    void setYSource(std::optional<ScatterAxisSource> source);

    // === Reference line ===
    [[nodiscard]] bool showReferenceLine() const { return _data.show_reference_line; }
    void setShowReferenceLine(bool show);

    // === Glyph style ===
    /** @brief Get glyph style state (for GlyphStyleControls binding) */
    [[nodiscard]] GlyphStyleState * glyphStyleState() { return _glyph_style_state.get(); }

    // === Serialization ===
    [[nodiscard]] std::string toJson() const override;
    bool fromJson(std::string const & json) override;

signals:
    void viewStateChanged();
    void xSourceChanged();
    void ySourceChanged();
    void referenceLineChanged();
    void glyphStyleChanged();

private:
    ScatterPlotStateData _data;
    std::unique_ptr<HorizontalAxisState> _horizontal_axis_state;
    std::unique_ptr<VerticalAxisState> _vertical_axis_state;
    std::unique_ptr<GlyphStyleState> _glyph_style_state;
};

#endif  // SCATTER_PLOT_STATE_HPP
