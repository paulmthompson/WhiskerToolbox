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

#include "Plots/EventPlotWidget/Core/EventPlotStateData.hpp"
#include "EditorState/EditorState.hpp"
#include "CorePlotting/CoordinateTransform/ViewStateData.hpp"
#include "Plots/Common/GlyphStyleWidget/Core/GlyphStyleState.hpp"
#include "Plots/Common/PlotAlignmentWidget/Core/PlotAlignmentData.hpp"
#include "Plots/Common/PlotAlignmentWidget/Core/PlotAlignmentState.hpp"
#include "Plots/Common/RelativeTimeAxisWidget/Core/RelativeTimeAxisStateData.hpp"
#include "Plots/Common/RelativeTimeAxisWidget/Core/RelativeTimeAxisState.hpp"

#include <rfl.hpp>
#include <rfl/json.hpp>

#include <map>
#include <memory>
#include <optional>
#include <string>
#include <vector>

/**
 * @brief State class for EventPlotWidget
 * 
 * EventPlotState is the Qt wrapper around EventPlotStateData that provides
 * typed accessors and Qt signals for all state properties.
 * 
 * EventPlotState uses composition with PlotAlignmentState to provide alignment
 * functionality via the shared PlotAlignmentWidget component.
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
    [[nodiscard]] QString getTypeName() const override { return QStringLiteral("EventPlotWidget"); }

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

    // === View State ===

    /**
     * @brief Get the view state (zoom, pan, bounds)
     * @return Const reference to the view state
     */
    [[nodiscard]] CorePlotting::ViewStateData const & viewState() const
    {
        return _data.view_state;
    }

    /**
     * @brief Set X-axis zoom factor (view transform only)
     * 
     * Adjusts magnification without changing the underlying data window.
     * Only emits viewStateChanged(), not stateChanged() - no scene rebuild.
     * 
     * @param zoom Zoom factor (1.0 = full window visible, 2.0 = half visible)
     */
    void setXZoom(double zoom);

    /**
     * @brief Set Y-axis zoom factor (view transform only)
     * 
     * Adjusts trial density without changing the underlying data.
     * Only emits viewStateChanged(), not stateChanged() - no scene rebuild.
     * 
     * @param zoom Zoom factor (1.0 = all trials fit, 2.0 = half the trials fit)
     */
    void setYZoom(double zoom);

    /**
     * @brief Set pan offset (view transform only)
     * 
     * Shifts the view without changing the underlying data window.
     * Only emits viewStateChanged(), not stateChanged() - no scene rebuild.
     * 
     * @param x_pan X-axis pan offset in world units (ms)
     * @param y_pan Y-axis pan offset in normalized units
     */
    void setPan(double x_pan, double y_pan);

    /**
     * @brief Set X-axis data bounds (triggers scene rebuild)
     * 
     * Changes the window of data gathered from the DataManager.
     * Emits both viewStateChanged() AND stateChanged() - triggers scene rebuild.
     * 
     * @param x_min Minimum time before alignment (typically negative)
     * @param x_max Maximum time after alignment (typically positive)
     */
    void setXBounds(double x_min, double x_max);

    // === Axis Options ===

    /**
     * @brief Get axis label and grid options
     * @return Const reference to axis options
     */
    [[nodiscard]] EventPlotAxisOptions const & axisOptions() const { return _data.axis_options; }

    /**
     * @brief Set axis options
     * @param options New axis options
     */
    void setAxisOptions(EventPlotAxisOptions const & options);

    // === Background Color ===

    /**
     * @brief Get the background color
     * @return Background color as hex string (e.g., "#FFFFFF")
     */
    [[nodiscard]] QString getBackgroundColor() const;

    /**
     * @brief Set the background color
     * @param hex_color Background color as hex string (e.g., "#FFFFFF")
     */
    void setBackgroundColor(QString const & hex_color);

    // === Pinning (for cross-widget linking) ===

    /**
     * @brief Check if the widget is pinned
     * @return true if pinned (ignores SelectionContext changes)
     */
    [[nodiscard]] bool isPinned() const { return _data.pinned; }

    /**
     * @brief Set the pinned state
     * @param pinned Whether to ignore SelectionContext changes
     */
    void setPinned(bool pinned);

    // === Trial Sorting ===

    /**
     * @brief Get the trial sorting mode
     * @return Current trial sorting mode
     */
    [[nodiscard]] TrialSortMode getSortingMode() const { return _data.sorting_mode; }

    /**
     * @brief Set the trial sorting mode
     * @param mode New sorting mode
     */
    void setSortingMode(TrialSortMode mode);

    // === Direct Data Access ===

    /**
     * @brief Get const reference to underlying data
     * @return Const reference to EventPlotStateData
     */
    [[nodiscard]] EventPlotStateData const & data() const { return _data; }

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

    // === Per-Key Glyph Style ===

    /**
     * @brief Get the GlyphStyleState for a specific plot event key.
     *
     * Returns nullptr if the key has not been added. The returned pointer is
     * owned by this state and must not be deleted by the caller.
     *
     * @param event_name The event name
     */
    [[nodiscard]] GlyphStyleState * glyphStyleStateForKey(QString const & event_name);

    /**
     * @brief Get the serializable glyph style data for a key (read-only).
     *
     * Returns a default style if the key has not been added.
     */
    [[nodiscard]] CorePlotting::GlyphStyleData getEventGlyphStyle(QString const & event_name) const;

    /**
     * @brief Update options for a specific plot event
     * @param event_name Name/key of the event
     * @param options New options (event_key only; glyph style is managed via GlyphStyleState)
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

    // === View State Signals (consolidated) ===

    /**
     * @brief Emitted when any view state property changes (zoom, pan, bounds)
     * 
     * This is a consolidated signal for all view state changes.
     * Connect to this for re-rendering the OpenGL view.
     */
    void viewStateChanged();

    /**
     * @brief Emitted when axis options change
     */
    void axisOptionsChanged();

    /**
     * @brief Emitted when background color changes
     * @param hex_color New background color as hex string
     */
    void backgroundColorChanged(QString const & hex_color);

    /**
     * @brief Emitted when pinned state changes
     * @param pinned New pinned state
     */
    void pinnedChanged(bool pinned);

    /**
     * @brief Emitted when trial sorting mode changes
     * @param mode New sorting mode
     */
    void sortingModeChanged(TrialSortMode mode);

    /**
     * @brief Emitted when the glyph style for any event key changes
     */
    void glyphStyleChanged();

    /**
     * @brief Emitted when the glyph style for a specific event key changes
     * @param event_name Name of the event whose style changed
     */
    void eventGlyphStyleChanged(QString const & event_name);

private:
    EventPlotStateData _data;
    std::unique_ptr<PlotAlignmentState> _alignment_state;
    std::unique_ptr<RelativeTimeAxisState> _relative_time_axis_state;
    /// Per-event GlyphStyleState objects (one per plot event key)
    std::map<std::string, std::unique_ptr<GlyphStyleState>> _event_glyph_style_states;

    /**
     * @brief Create a GlyphStyleState for a newly added event key.
     *
     * Looks up existing serialized style from _data.event_glyph_styles,
     * or uses the default style. Connects signals for scene rebuild.
     */
    void _createGlyphStyleStateForKey(std::string const & key);
};

#endif// EVENT_PLOT_STATE_HPP
