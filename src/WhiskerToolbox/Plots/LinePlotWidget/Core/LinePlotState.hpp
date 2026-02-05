#ifndef LINE_PLOT_STATE_HPP
#define LINE_PLOT_STATE_HPP

/**
 * @file LinePlotState.hpp
 * @brief State class for LinePlotWidget
 * 
 * LinePlotState manages the serializable state for the LinePlotWidget,
 * enabling workspace save/restore and inter-widget communication via SelectionContext.
 * 
 * @see EditorState for base class documentation
 */

#include "EditorState/EditorState.hpp"
#include "Plots/Common/PlotAlignmentWidget/Core/PlotAlignmentData.hpp"
#include "Plots/Common/PlotAlignmentWidget/Core/PlotAlignmentState.hpp"

#include <rfl.hpp>
#include <rfl/json.hpp>

#include <map>
#include <memory>
#include <optional>
#include <string>
#include <vector>

/**
 * @brief Options for plotting an analog time series in the line plot
 */
struct LinePlotOptions {
    std::string series_key;                           ///< Key of the AnalogTimeSeries to plot
    double line_thickness = 1.0;                      ///< Thickness of the line (default: 1.0)
    std::string hex_color = "#000000";                ///< Color as hex string (default: black)
};

/**
 * @brief Serializable state data for LinePlotWidget
 */
struct LinePlotStateData {
    std::string instance_id;
    std::string display_name = "Line Plot";
    PlotAlignmentData alignment;                                                      ///< Alignment settings (event key, interval type, offset, window size)
    std::map<std::string, LinePlotOptions> plot_series;                              ///< Map of series names to their plot options
};

/**
 * @brief State class for LinePlotWidget
 * 
 * LinePlotState is the Qt wrapper around LinePlotStateData that provides
 * typed accessors and Qt signals for all state properties.
 * 
 * LinePlotState uses composition with PlotAlignmentState to provide alignment
 * functionality via the shared PlotAlignmentWidget component.
 */
class LinePlotState : public EditorState {
    Q_OBJECT

public:
    /**
     * @brief Construct a new LinePlotState
     * @param parent Parent QObject (typically nullptr, managed by WorkspaceManager)
     */
    explicit LinePlotState(QObject * parent = nullptr);

    ~LinePlotState() override = default;

    // === Type Identification ===

    /**
     * @brief Get the type name for this state
     * @return "LinePlot"
     */
    [[nodiscard]] QString getTypeName() const override { return QStringLiteral("LinePlot"); }

    /**
     * @brief Get the display name for UI
     * @return User-visible name (default: "Line Plot")
     */
    [[nodiscard]] QString getDisplayName() const override;

    /**
     * @brief Set the display name
     * @param name New display name
     */
    void setDisplayName(QString const & name) override;

    // === Alignment Event ===

    /**
     * @brief Get the alignment event key
     * @return Key of the selected event/interval series
     */
    [[nodiscard]] QString getAlignmentEventKey() const;

    /**
     * @brief Set the alignment event key
     * @param key Key of the event/interval series to use for alignment
     */
    void setAlignmentEventKey(QString const & key);

    // === Interval Alignment ===

    /**
     * @brief Get the interval alignment type
     * @return Whether to use beginning or end of intervals
     */
    [[nodiscard]] IntervalAlignmentType getIntervalAlignmentType() const;

    /**
     * @brief Set the interval alignment type
     * @param type Whether to use beginning or end of intervals
     */
    void setIntervalAlignmentType(IntervalAlignmentType type);

    // === Offset ===

    /**
     * @brief Get the offset value
     * @return Offset in time units
     */
    [[nodiscard]] double getOffset() const;

    /**
     * @brief Set the offset value
     * @param offset Offset in time units to apply to alignment events
     */
    void setOffset(double offset);

    // === Window Size ===

    /**
     * @brief Get the window size
     * @return Window size in time units
     */
    [[nodiscard]] double getWindowSize() const;

    /**
     * @brief Set the window size
     * @param window_size Window size in time units to gather around alignment event
     */
    void setWindowSize(double window_size);

    /**
     * @brief Get the alignment state object
     * @return Pointer to the alignment state (for use with PlotAlignmentWidget)
     */
    [[nodiscard]] PlotAlignmentState * alignmentState() { return _alignment_state.get(); }

    // === Plot Series Management ===

    /**
     * @brief Add a series to the plot
     * @param series_name Name/key for the series (used as identifier)
     * @param series_key DataManager key of the AnalogTimeSeries
     */
    void addPlotSeries(QString const & series_name, QString const & series_key);

    /**
     * @brief Remove a series from the plot
     * @param series_name Name/key of the series to remove
     */
    void removePlotSeries(QString const & series_name);

    /**
     * @brief Get all plot series names
     * @return List of series names currently in the plot
     */
    [[nodiscard]] std::vector<QString> getPlotSeriesNames() const;

    /**
     * @brief Get options for a specific plot series
     * @param series_name Name/key of the series
     * @return Options struct, or std::nullopt if series not found
     */
    [[nodiscard]] std::optional<LinePlotOptions> getPlotSeriesOptions(QString const & series_name) const;

    /**
     * @brief Update options for a specific plot series
     * @param series_name Name/key of the series
     * @param options New options
     */
    void updatePlotSeriesOptions(QString const & series_name, LinePlotOptions const & options);

    // === Serialization ===

    /**
     * @brief Serialize state to JSON
     * @return JSON string representation
     */
    [[nodiscard]] std::string toJson() const override;

    /**
     * @brief Restore state from JSON
     * @param json JSON string to parse
     * @return true if parsing succeeded
     */
    bool fromJson(std::string const & json) override;

signals:
    /**
     * @brief Emitted when alignment event key changes
     * @param key New alignment event key
     */
    void alignmentEventKeyChanged(QString const & key);

    /**
     * @brief Emitted when interval alignment type changes
     * @param type New interval alignment type
     */
    void intervalAlignmentTypeChanged(IntervalAlignmentType type);

    /**
     * @brief Emitted when offset changes
     * @param offset New offset value
     */
    void offsetChanged(double offset);

    /**
     * @brief Emitted when window size changes
     * @param window_size New window size value
     */
    void windowSizeChanged(double window_size);

    /**
     * @brief Emitted when a plot series is added
     * @param series_name Name of the added series
     */
    void plotSeriesAdded(QString const & series_name);

    /**
     * @brief Emitted when a plot series is removed
     * @param series_name Name of the removed series
     */
    void plotSeriesRemoved(QString const & series_name);

    /**
     * @brief Emitted when plot series options are updated
     * @param series_name Name of the updated series
     */
    void plotSeriesOptionsChanged(QString const & series_name);

private:
    LinePlotStateData _data;
    std::unique_ptr<PlotAlignmentState> _alignment_state;
};

#endif// LINE_PLOT_STATE_HPP
