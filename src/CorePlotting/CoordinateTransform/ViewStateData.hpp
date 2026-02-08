#ifndef COREPLOTTING_COORDINATETRANSFORM_VIEWSTATEDATA_HPP
#define COREPLOTTING_COORDINATETRANSFORM_VIEWSTATEDATA_HPP

/**
 * @file ViewStateData.hpp
 * @brief Serializable subset of the 2D camera / viewport state.
 *
 * ViewStateData captures the *logical* view configuration (zoom, pan, data
 * bounds) without any runtime-only information (viewport pixel dimensions,
 * validity flag).  It is designed to be:
 *
 * - Embedded directly in per-widget StateData structs for workspace
 *   save/restore via reflect-cpp (rfl::json).
 * - Passed to PlotInteractionHelpers as a ViewStateLike type (it exposes
 *   x_zoom, y_zoom, x_pan, y_pan).
 * - Trivially promoted to the full CorePlotting::ViewState at render time
 *   by combining it with the current viewport dimensions.
 *
 * This replaces the proliferation of identical per-widget ViewState structs
 * (EventPlotViewState, PSTHViewState, LinePlotViewState, HeatmapViewState,
 * SpectrogramViewState, ScatterPlotViewState, ACFViewState,
 * TemporalProjectionViewViewState).
 *
 * @see CorePlotting::ViewState        — full runtime struct (includes viewport)
 * @see CorePlotting::toRuntimeViewState — ViewStateData → ViewState conversion
 */

namespace CorePlotting {

/**
 * @brief Serializable 2D camera state.
 *
 * All fields use double for serialization precision.
 *
 * ### Data Bounds
 *
 * `x_min`/`x_max` and `y_min`/`y_max` define the world-coordinate extent of
 * the data being visualised.  Changing these typically triggers a scene rebuild
 * (e.g. re-gathering trial data for a different time window).
 *
 * For widgets whose bounds come from a separate AxisState (ScatterPlot, ACF,
 * TemporalProjection), bounds can be left at their defaults and set at
 * render time.
 *
 * ### View Transform
 *
 * `x_zoom`/`y_zoom` scale the visible range (1.0 = fit-to-bounds).
 * `x_pan`/`y_pan` shift the view in world coordinates.
 * Changing these only requires a projection matrix update, not a scene rebuild.
 */
struct ViewStateData {
    // === Data Bounds (changing these triggers scene rebuild) ===
    double x_min = -500.0;
    double x_max =  500.0;
    double y_min =    0.0;
    double y_max =  100.0;

    // === View Transform (changing these only updates projection matrix) ===
    double x_zoom = 1.0;
    double y_zoom = 1.0;
    double x_pan  = 0.0;
    double y_pan  = 0.0;
};

} // namespace CorePlotting

#endif // COREPLOTTING_COORDINATETRANSFORM_VIEWSTATEDATA_HPP
