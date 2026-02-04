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

#include <string>

/**
 * @brief Enumeration for interval alignment type
 */
enum class IntervalAlignmentType {
    Beginning,
    End
};

/**
 * @brief Serializable state data for EventPlotWidget
 */
struct EventPlotStateData {
    std::string instance_id;
    std::string display_name = "Event Plot";
    std::string plot_event_key;  ///< Key of the DigitalEventSeries to plot in the raster
    std::string alignment_event_key;  ///< Key of the selected event/interval series for alignment
    IntervalAlignmentType interval_alignment_type = IntervalAlignmentType::Beginning;  ///< For intervals: use beginning or end
    double offset = 0.0;  ///< Offset in time units to apply to alignment events
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

    // === Plot Event ===

    /**
     * @brief Get the plot event key
     * @return Key of the selected DigitalEventSeries to plot
     */
    [[nodiscard]] QString getPlotEventKey() const;

    /**
     * @brief Set the plot event key
     * @param key Key of the DigitalEventSeries to plot in the raster
     */
    void setPlotEventKey(QString const & key);

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
     * @brief Emitted when plot event key changes
     * @param key New plot event key
     */
    void plotEventKeyChanged(QString const & key);

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

private:
    EventPlotStateData _data;
};

#endif  // EVENT_PLOT_STATE_HPP
