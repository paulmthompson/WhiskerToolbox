#ifndef EVENT_PLOT_STATE_HPP
#define EVENT_PLOT_STATE_HPP

/**
 * @file EventPlotState.hpp
 * @brief State class for EventPlotWidget
 * 
 * EventPlotState manages the serializable state for the EventPlotWidget,
 * enabling workspace save/restore and inter-widget communication via SelectionContext.
 * 
 * @see EditorState for base class documentation
 */

#include "EditorState/EditorState.hpp"

#include <rfl.hpp>
#include <rfl/json.hpp>

#include <map>
#include <optional>
#include <string>
#include <vector>

/**
 * @brief Enumeration for interval alignment type
 */
enum class IntervalAlignmentType {
    Beginning,
    End
};

/**
 * @brief Options for plotting an event series in the raster plot
 */
struct EventPlotOptions {
    std::string event_key;  ///< Key of the DigitalEventSeries to plot
    // Additional options can be added here in the future (e.g., color, style, etc.)
};

/**
 * @brief Serializable state data for EventPlotWidget
 */
struct EventPlotStateData {
    std::string instance_id;
    std::string display_name = "Event Plot";
    std::string alignment_event_key;  ///< Key of the selected event/interval series for alignment
    IntervalAlignmentType interval_alignment_type = IntervalAlignmentType::Beginning;  ///< For intervals: use beginning or end
    double offset = 0.0;  ///< Offset in time units to apply to alignment events
    double window_size = 1000.0;  ///< Window size in time units to gather around alignment event
    std::map<std::string, EventPlotOptions> plot_events;  ///< Map of event names to their plot options
};

/**
 * @brief State class for EventPlotWidget
 * 
 * EventPlotState is the Qt wrapper around EventPlotStateData that provides
 * typed accessors and Qt signals for all state properties.
 */
class EventPlotState : public EditorState {
    Q_OBJECT

public:
    /**
     * @brief Construct a new EventPlotState
     * @param parent Parent QObject (typically nullptr, managed by WorkspaceManager)
     */
    explicit EventPlotState(QObject * parent = nullptr);

    ~EventPlotState() override = default;

    // === Type Identification ===

    /**
     * @brief Get the type name for this state
     * @return "EventPlot"
     */
    [[nodiscard]] QString getTypeName() const override { return QStringLiteral("EventPlot"); }

    /**
     * @brief Get the display name for UI
     * @return User-visible name (default: "Event Plot")
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
    [[nodiscard]] std::optional<EventPlotOptions> getPlotEventOptions(QString const & event_name) const;

    /**
     * @brief Update options for a specific plot event
     * @param event_name Name/key of the event
     * @param options New options
     */
    void updatePlotEventOptions(QString const & event_name, EventPlotOptions const & options);

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

private:
    EventPlotStateData _data;
};

#endif  // EVENT_PLOT_STATE_HPP
