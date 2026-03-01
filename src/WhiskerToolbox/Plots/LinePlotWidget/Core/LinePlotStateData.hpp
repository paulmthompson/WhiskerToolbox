#ifndef LINE_PLOT_STATE_DATA_HPP
#define LINE_PLOT_STATE_DATA_HPP

/**
 * @file LinePlotStateData.hpp
 * @brief Serializable state data structure for LinePlotWidget
 *
 * Separated from LinePlotState.hpp so it can be included without
 * pulling in Qt dependencies (e.g. for fuzz testing).
 *
 * @see LinePlotState for the Qt wrapper class
 */

#include "CorePlotting/CoordinateTransform/ViewStateData.hpp"
#include "CorePlotting/DataTypes/LineStyleData.hpp"
#include "Plots/Common/PlotAlignmentWidget/Core/PlotAlignmentData.hpp"
#include "Plots/Common/RelativeTimeAxisWidget/Core/RelativeTimeAxisStateData.hpp"
#include "Plots/Common/VerticalAxisWidget/Core/VerticalAxisStateData.hpp"

#include <rfl.hpp>

#include <map>
#include <string>

/**
 * @brief Options for plotting an analog time series in the line plot
 *
 * Uses the shared LineStyleData for color/thickness/alpha configuration,
 * which enables use of LineStyleControls in the properties panel.
 */
struct LinePlotOptions {
    std::string series_key;                              ///< Key of the AnalogTimeSeries to plot
    CorePlotting::LineStyleData line_style;              ///< Style (color, thickness, alpha)
};

/**
 * @brief Serializable state data for LinePlotWidget
 */
struct LinePlotStateData {
    std::string instance_id;
    std::string display_name = "Line Plot";
    PlotAlignmentData alignment;                                                      ///< Alignment settings (event key, interval type, offset, window size)
    std::map<std::string, LinePlotOptions> plot_series;                               ///< Map of series names to their plot options
    CorePlotting::ViewStateData view_state;                                            ///< Zoom, pan, data bounds
    RelativeTimeAxisStateData time_axis;                                              ///< Time axis settings (min_range, max_range)
    VerticalAxisStateData vertical_axis;                                              ///< Vertical axis settings (y_min, y_max)
};

#endif // LINE_PLOT_STATE_DATA_HPP
