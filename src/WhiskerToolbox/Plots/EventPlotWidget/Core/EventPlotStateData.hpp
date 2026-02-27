#ifndef EVENT_PLOT_STATE_DATA_HPP
#define EVENT_PLOT_STATE_DATA_HPP

/**
 * @file EventPlotStateData.hpp
 * @brief Serializable state data structure for EventPlotWidget
 *
 * Separated from EventPlotState.hpp so it can be included without
 * pulling in Qt dependencies (e.g. for fuzz testing).
 *
 * @see EventPlotState for the Qt wrapper class
 */

#include "CorePlotting/CoordinateTransform/ViewStateData.hpp"
#include "CorePlotting/DataTypes/GlyphStyleData.hpp"
#include "Plots/Common/PlotAlignmentWidget/Core/PlotAlignmentData.hpp"
#include "Plots/Common/RelativeTimeAxisWidget/Core/RelativeTimeAxisStateData.hpp"

#include <rfl.hpp>

#include <map>
#include <optional>
#include <string>

/**
 * @brief Enumeration for trial sorting modes
 *
 * Defines how trials are sorted in the raster plot. Built-in modes compute
 * sort keys directly from the GatherResult data. External mode is reserved
 * for future integration with DataTransform v2 for user-computed sort keys.
 */
enum class TrialSortMode {
    TrialIndex,         ///< No sorting - display in original trial order (default)
    FirstEventLatency,  ///< Sort by latency to first positive event (ascending)
    EventCount          ///< Sort by total number of events (descending)
    // Future: External - sort by external feature from DataManager
};

/**
 * @brief Axis labeling and grid options
 */
struct EventPlotAxisOptions {
    std::string x_label = "Time (ms)";  ///< X-axis label
    std::string y_label = "Trial";       ///< Y-axis label
    bool show_x_axis = true;             ///< Whether to show X axis
    bool show_y_axis = true;             ///< Whether to show Y axis
    bool show_grid = false;              ///< Whether to show grid lines
};;

/**
 * @brief Options for plotting an event series in the raster plot
 *
 * Event key plus optional metadata. Glyph styling is stored separately
 * in the per-key glyph style map (`event_glyph_styles`) so that
 * GlyphStyleState objects can manage live state and serialization.
 *
 * When the key refers to a DigitalIntervalSeries, `interval_edge` specifies
 * which edge (start or end) to extract as events for plotting.
 */
struct EventPlotOptions {
    std::string event_key;  ///< Key of the DigitalEventSeries or DigitalIntervalSeries to plot
    std::optional<IntervalAlignmentType> interval_edge;  ///< Edge to extract when source is an interval series
};

/**
 * @brief Serializable state data for EventPlotWidget
 */
struct EventPlotStateData {
    std::string instance_id;
    std::string display_name = "Event Plot";
    PlotAlignmentData alignment;                                   ///< Alignment settings (event key, interval type, offset, window size)
    std::map<std::string, EventPlotOptions> plot_events;          ///< Map of event names to their plot options
    std::map<std::string, CorePlotting::GlyphStyleData> event_glyph_styles;  ///< Per-event glyph styles (key → GlyphStyleData)
    CorePlotting::ViewStateData view_state;                        ///< View state (zoom, pan, bounds). Y bounds fixed at -1..1 for trial viewport
    RelativeTimeAxisStateData time_axis;                           ///< Time axis settings (min_range, max_range)
    EventPlotAxisOptions axis_options;                             ///< Axis labels and grid options
    std::string background_color = "#FFFFFF";                     ///< Background color as hex string (default: white)
    bool pinned = false;                                           ///< Whether to ignore SelectionContext changes
    TrialSortMode sorting_mode = TrialSortMode::TrialIndex;       ///< Trial sorting mode (default: trial index)
};

#endif // EVENT_PLOT_STATE_DATA_HPP
