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
#include "Plots/Common/PlotAlignmentWidget/Core/PlotAlignmentData.hpp"
#include "Plots/Common/RelativeTimeAxisWidget/Core/RelativeTimeAxisStateData.hpp"

#include <rfl.hpp>

#include <map>
#include <string>

/**
 * @brief Enumeration for event glyph/marker type
 */
enum class EventGlyphType {
    Tick,  ///< Vertical line (default)
    Circle,///< Circle marker
    Square ///< Square marker
};

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
 */
struct EventPlotOptions {
    std::string event_key;                           ///< Key of the DigitalEventSeries to plot
    double tick_thickness = 2.0;                     ///< Thickness of the tick/glyph (default: 2.0)
    EventGlyphType glyph_type = EventGlyphType::Tick;///< Type of glyph to display (default: Tick/vertical line)
    std::string hex_color = "#000000";               ///< Color as hex string (default: black)
};

/**
 * @brief Serializable state data for EventPlotWidget
 */
struct EventPlotStateData {
    std::string instance_id;
    std::string display_name = "Event Plot";
    PlotAlignmentData alignment;                                   ///< Alignment settings (event key, interval type, offset, window size)
    std::map<std::string, EventPlotOptions> plot_events;          ///< Map of event names to their plot options
    CorePlotting::ViewStateData view_state;                        ///< View state (zoom, pan, bounds). Y bounds fixed at -1..1 for trial viewport
    RelativeTimeAxisStateData time_axis;                           ///< Time axis settings (min_range, max_range)
    EventPlotAxisOptions axis_options;                             ///< Axis labels and grid options
    std::string background_color = "#FFFFFF";                     ///< Background color as hex string (default: white)
    bool pinned = false;                                           ///< Whether to ignore SelectionContext changes
    TrialSortMode sorting_mode = TrialSortMode::TrialIndex;       ///< Trial sorting mode (default: trial index)
};

#endif // EVENT_PLOT_STATE_DATA_HPP
