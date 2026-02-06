#ifndef PSTH_STATE_HPP
#define PSTH_STATE_HPP

/**
 * @file PSTHState.hpp
 * @brief State class for PSTHWidget
 * 
 * PSTHState manages the serializable state for the PSTHWidget,
 * enabling workspace save/restore and inter-widget communication via SelectionContext.
 * 
 * @see EditorState for base class documentation
 */

#include "EditorState/EditorState.hpp"
#include "Plots/Common/PlotAlignmentWidget/Core/PlotAlignmentData.hpp"
#include "Plots/Common/PlotAlignmentWidget/Core/PlotAlignmentState.hpp"
#include "Plots/Common/RelativeTimeAxisWidget/Core/RelativeTimeAxisStateData.hpp"
#include "Plots/Common/RelativeTimeAxisWidget/Core/RelativeTimeAxisState.hpp"
#include "Plots/Common/VerticalAxisWidget/Core/VerticalAxisStateData.hpp"
#include "Plots/Common/VerticalAxisWidget/Core/VerticalAxisState.hpp"

#include <rfl.hpp>
#include <rfl/json.hpp>

#include <map>
#include <memory>
#include <optional>
#include <string>
#include <vector>


/**
 * @brief Enumeration for PSTH plot style
 */
enum class PSTHStyle {
    Bar,  ///< Bar chart style (default)
    Line  ///< Line plot style
};

/**
 * @brief Options for plotting an event series in the PSTH plot
 */
struct PSTHEventOptions {
    std::string event_key;              ///< Key of the DigitalEventSeries to plot
    std::string hex_color = "#000000";  ///< Color as hex string (default: black)
};

/**
 * @brief Serializable state data for PSTHWidget
 */
struct PSTHStateData {
    std::string instance_id;
    std::string display_name = "PSTH Plot";
    PlotAlignmentData alignment;                                    ///< Alignment settings (event key, interval type, offset, window size)
    std::map<std::string, PSTHEventOptions> plot_events;           ///< Map of event names to their plot options
    PSTHStyle style = PSTHStyle::Bar;                              ///< Plot style (bar or line)
    double bin_size = 10.0;                                         ///< Bin size in time units (default: 10.0)
    RelativeTimeAxisStateData time_axis;                            ///< Time axis settings (min_range, max_range)
    VerticalAxisStateData vertical_axis;                           ///< Vertical axis settings (y_min, y_max)
};

/**
 * @brief State class for PSTHWidget
 * 
 * PSTHState is the Qt wrapper around PSTHStateData that provides
 * typed accessors and Qt signals for all state properties.
 * 
 * PSTHState uses composition with PlotAlignmentState to provide alignment
 * functionality via the shared PlotAlignmentWidget component.
 */
class PSTHState : public EditorState {
    Q_OBJECT

public:
    /**
     * @brief Construct a new PSTHState
     * @param parent Parent QObject (typically nullptr, managed by WorkspaceManager)
     */
    explicit PSTHState(QObject * parent = nullptr);

    ~PSTHState() override = default;

    // === Type Identification ===

    /**
     * @brief Get the type name for this state
     * @return "PSTH"
     */
    [[nodiscard]] QString getTypeName() const override { return QStringLiteral("PSTH"); }

    /**
     * @brief Get the display name for UI
     * @return User-visible name (default: "PSTH Plot")
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

    /**
     * @brief Get the relative time axis state object
     * @return Pointer to the relative time axis state
     */
    [[nodiscard]] RelativeTimeAxisState * relativeTimeAxisState() { return _relative_time_axis_state.get(); }

    /**
     * @brief Get the vertical axis state object
     * @return Pointer to the vertical axis state
     */
    [[nodiscard]] VerticalAxisState * verticalAxisState() { return _vertical_axis_state.get(); }

    // === Plot Events Management ===

    /**
     * @brief Add an event to the plot
     * @param event_name Name/key for the event (used as identifier)
     * @param event_key DataManager key of the DigitalEventSeries
     */
    void addPlotEvent(QString const & event_name, QString const & event_key);

    /**
     * @brief Remove an event from the plot
     * @param event_name Name/key of the event to remove
     */
    void removePlotEvent(QString const & event_name);

    /**
     * @brief Get all plot event names
     * @return List of event names currently in the plot
     */
    [[nodiscard]] std::vector<QString> getPlotEventNames() const;

    /**
     * @brief Get options for a specific plot event
     * @param event_name Name/key of the event
     * @return Options struct, or std::nullopt if event not found
     */
    [[nodiscard]] std::optional<PSTHEventOptions> getPlotEventOptions(QString const & event_name) const;

    /**
     * @brief Update options for a specific plot event
     * @param event_name Name/key of the event
     * @param options New options
     */
    void updatePlotEventOptions(QString const & event_name, PSTHEventOptions const & options);

    // === Global Plot Options ===

    /**
     * @brief Get the plot style
     * @return Current plot style (Bar or Line)
     */
    [[nodiscard]] PSTHStyle getStyle() const;

    /**
     * @brief Set the plot style
     * @param style New plot style
     */
    void setStyle(PSTHStyle style);

    /**
     * @brief Get the bin size
     * @return Bin size in time units
     */
    [[nodiscard]] double getBinSize() const;

    /**
     * @brief Set the bin size
     * @param bin_size Bin size in time units
     */
    void setBinSize(double bin_size);

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
     * @brief Emitted when a plot event is added
     * @param event_name Name of the added event
     */
    void plotEventAdded(QString const & event_name);

    /**
     * @brief Emitted when a plot event is removed
     * @param event_name Name of the removed event
     */
    void plotEventRemoved(QString const & event_name);

    /**
     * @brief Emitted when plot event options are updated
     * @param event_name Name of the updated event
     */
    void plotEventOptionsChanged(QString const & event_name);

    /**
     * @brief Emitted when plot style changes
     * @param style New plot style
     */
    void styleChanged(PSTHStyle style);

    /**
     * @brief Emitted when bin size changes
     * @param bin_size New bin size value
     */
    void binSizeChanged(double bin_size);

private:
    PSTHStateData _data;
    std::unique_ptr<PlotAlignmentState> _alignment_state;
    std::unique_ptr<RelativeTimeAxisState> _relative_time_axis_state;
    std::unique_ptr<VerticalAxisState> _vertical_axis_state;
};

#endif// PSTH_STATE_HPP
