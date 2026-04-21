#ifndef DATA_VIEWER_STATE_HPP
#define DATA_VIEWER_STATE_HPP

/**
 * @file DataViewerState.hpp
 * @brief State class for DataViewer_Widget
 * 
 * DataViewerState manages the serializable state for the DataViewer_Widget,
 * enabling workspace save/restore and inter-widget communication via SelectionContext.
 * 
 * ## Design Pattern
 * 
 * This class follows the same pattern as MediaWidgetState:
 * - Inherits from EditorState for common functionality
 * - Uses DataViewerStateData for reflect-cpp serialization
 * - Uses SeriesOptionsRegistry for per-series display options
 * - Emits consolidated Qt signals for state changes
 * 
 * ## State Categories
 * 
 * | Category | Description | Example Properties |
 * |----------|-------------|--------------------|
 * | View State | Time window, Y bounds, scale | time_start, time_end, global_y_scale |
 * | Theme | Visual appearance | theme (Dark/Light), background_color |
 * | Grid | Grid overlay settings | enabled, spacing |
 * | UI Preferences | Widget layout | zoom_scaling_mode, panel_collapsed |
 * | Interaction | Current tool mode | Normal, CreateInterval, etc. |
 * | Series Options | Per-series display | color, visibility, scale per key |
 * 
 * ## Usage
 * 
 * ```cpp
 * // Create state
 * auto state = std::make_shared<DataViewerState>();
 * 
 * // === View State ===
 * state->setTimeWindow(0, 10000);
 * state->setGlobalYScale(1.5);
 * 
 * // === Series Options ===
 * AnalogSeriesOptionsData opts;
 * opts.hex_color() = "#ff0000";
 * opts.user_scale_factor = 2.0f;
 * state->seriesOptions().set("channel_1", opts);
 * 
 * // === Theme ===
 * state->setTheme("Dark");
 * state->setBackgroundColor("#1a1a1a");
 * 
 * // === Serialization ===
 * std::string json = state->toJson();
 * state->fromJson(json);
 * ```
 * 
 * @see EditorState for base class documentation
 * @see DataViewerStateData for the complete state structure
 * @see SeriesOptionsRegistry for per-series options API
 */

#include "DataViewerStateData.hpp"
#include "EditorState/EditorState.hpp"
#include "EditorState/StrongTypes.hpp"// Must be before any TimePosition usage in signals
#include "SeriesOptionsRegistry.hpp"
#include "TimeFrame/TimeFrame.hpp"

#include "CorePlotting/CoordinateTransform/ViewStateData.hpp"

#include <rfl.hpp>
#include <rfl/json.hpp>

#include <QStringList>

#include <cstdint>
#include <map>
#include <memory>
#include <string>
#include <utility>
#include <vector>

class MultiLaneVerticalAxisState;

/**
 * @brief State class for DataViewer_Widget
 * 
 * DataViewerState is the Qt wrapper around DataViewerStateData that provides
 * typed accessors and Qt signals for all state properties.
 * 
 * ## Signal Categories
 * 
 * - **View State**: viewStateChanged
 * - **Theme**: themeChanged
 * - **Grid**: gridChanged
 * - **UI Preferences**: uiPreferencesChanged
 * - **Interaction**: interactionModeChanged
 * - **Series Options**: Forwarded from SeriesOptionsRegistry
 * 
 * ## Thread Safety
 * 
 * This class is NOT thread-safe. All access should be from the main/GUI thread.
 */
class DataViewerState : public EditorState {
    Q_OBJECT

public:
    /**
     * @brief Construct a new DataViewerState
     * @param parent Parent QObject (typically nullptr, managed by WorkspaceManager)
     */
    explicit DataViewerState(QObject * parent = nullptr);

    ~DataViewerState() override;

    // === Type Identification ===

    /**
     * @brief Get the type name for this state
     * @return "DataViewerWidget" — must match the type_id used in DataViewerWidgetRegistration
     */
    [[nodiscard]] QString getTypeName() const override { return QStringLiteral("DataViewerWidget"); }

    /**
     * @brief Get the display name for UI
     * @return User-visible name (default: "Data Viewer")
     */
    [[nodiscard]] QString getDisplayName() const override;

    /**
     * @brief Set the display name
     * @param name New display name
     */
    void setDisplayName(QString const & name) override;

    // === Transient Runtime State ===
    // (NOT serialized - just runtime)
    TimePosition current_position;

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

    // === Direct Data Access ===

    /**
     * @brief Get const reference to underlying data for efficiency
     * 
     * Use this for reading multiple values without individual accessor overhead.
     * For modification, use the typed setters to ensure signals are emitted.
     * 
     * @return Const reference to DataViewerStateData
     */
    [[nodiscard]] DataViewerStateData const & data() const { return _data; }

    // === Series Options Registry ===

    /**
     * @brief Get the series options registry for generic access
     * 
     * The registry provides a unified API for all series option types:
     * 
     * ```cpp
     * // Set options (type inferred)
     * AnalogSeriesOptionsData opts;
     * opts.hex_color() = "#ff0000";
     * state->seriesOptions().set("channel_1", opts);
     * 
     * // Get options (type explicit)
     * auto* opts = state->seriesOptions().get<AnalogSeriesOptionsData>("channel_1");
     * 
     * // Get visible keys
     * QStringList visible = state->seriesOptions().visibleKeys<AnalogSeriesOptionsData>();
     * ```
     * 
     * @return Reference to SeriesOptionsRegistry
     */
    [[nodiscard]] SeriesOptionsRegistry & seriesOptions() { return _series_options; }
    [[nodiscard]] SeriesOptionsRegistry const & seriesOptions() const { return _series_options; }

    // ==================== View State ====================

    /**
     * @brief Set the visible time window
     * @param start Start of time window (TimeFrameIndex units)
     * @param end End of time window (inclusive)
     */
    void setTimeWindow(int64_t start, int64_t end);

    /**
     * @brief Get the visible time window
     * @return Pair of (start, end) time values
     */
    [[nodiscard]] std::pair<int64_t, int64_t> timeWindow() const;

    /**
     * @brief Adjust time window width by a delta while keeping center fixed
     * 
     * Positive delta increases the range width (zoom out).
     * Negative delta decreases the range width (zoom in).
     * Emits viewStateChanged signal.
     * 
     * @param delta Amount to add to current width (negative = zoom in)
     */
    void adjustTimeWidth(int64_t delta);

    /**
     * @brief Set time window width to a specific value while keeping center fixed
     * 
     * Emits viewStateChanged signal.
     * 
     * @param width Desired width of visible range (minimum 1)
     * @return Actual width achieved (will be at least 1)
     */
    int64_t setTimeWidth(int64_t width);

    /**
     * @brief Set the Y-axis bounds
     * @param y_min Minimum Y value in normalized device coordinates
     * @param y_max Maximum Y value in normalized device coordinates
     */
    void setYBounds(float y_min, float y_max);

    /**
     * @brief Get the Y-axis bounds
     * @return Pair of (y_min, y_max)
     */
    [[nodiscard]] std::pair<float, float> yBounds() const;

    /**
     * @brief Set the vertical pan offset
     * @param offset Offset in NDC units
     */
    void setVerticalPanOffset(float offset);

    /**
     * @brief Get the vertical pan offset
     * @return Current vertical pan offset
     */
    [[nodiscard]] float verticalPanOffset() const { return static_cast<float>(_data.view.y_pan); }

    /**
     * @brief Set the global Y-axis scale
     * @param scale Y-axis amplitude scale factor (affects all series uniformly)
     */
    void setGlobalYScale(float scale);

    /**
     * @brief Get the global Y-axis scale
     * @return Current global Y-axis scale factor
     */
    [[nodiscard]] float globalYScale() const { return _data.global_y_scale; }

    // ==================== Convenience Time Accessors ====================

    /**
     * @brief Get the start of the visible time window as integer index
     */
    [[nodiscard]] int64_t timeStart() const { return static_cast<int64_t>(_data.view.x_min); }

    /**
     * @brief Get the end of the visible time window as integer index
     */
    [[nodiscard]] int64_t timeEnd() const { return static_cast<int64_t>(_data.view.x_max); }

    /**
     * @brief Get visible time window width
     */
    [[nodiscard]] int64_t timeWidth() const { return timeEnd() - timeStart() + 1; }

    /**
     * @brief Get center of visible time window
     */
    [[nodiscard]] int64_t timeCenter() const { return timeStart() + (timeEnd() - timeStart()) / 2; }

    /**
     * @brief Set the complete view state
     * @param view New view state
     */
    void setViewState(CorePlotting::ViewStateData const & view);

    /**
     * @brief Get the complete view state
     * @return Const reference to CorePlotting::ViewStateData
     */
    [[nodiscard]] CorePlotting::ViewStateData const & viewState() const { return _data.view; }

    // ==================== Theme ====================

    /**
     * @brief Set the visual theme
     * @param theme Theme enum value
     */
    void setTheme(DataViewerTheme theme);

    /**
     * @brief Get the visual theme
     * @return Current theme
     */
    [[nodiscard]] DataViewerTheme theme() const { return _data.theme.theme; }

    /**
     * @brief Set the background color
     * @param hex Color in hex format (e.g., "#000000")
     */
    void setBackgroundColor(QString const & hex);

    /**
     * @brief Get the background color
     * @return Color in hex format
     */
    [[nodiscard]] QString backgroundColor() const { return QString::fromStdString(_data.theme.background_color); }

    /**
     * @brief Set the axis/text color
     * @param hex Color in hex format (e.g., "#FFFFFF")
     */
    void setAxisColor(QString const & hex);

    /**
     * @brief Get the axis/text color
     * @return Color in hex format
     */
    [[nodiscard]] QString axisColor() const { return QString::fromStdString(_data.theme.axis_color); }

    /**
     * @brief Set the complete theme state
     * @param theme_state New theme state
     */
    void setThemeState(DataViewerThemeState const & theme_state);

    /**
     * @brief Get the complete theme state
     * @return Const reference to DataViewerThemeState
     */
    [[nodiscard]] DataViewerThemeState const & themeState() const { return _data.theme; }

    // ==================== Grid ====================

    /**
     * @brief Enable or disable the grid overlay
     * @param enabled Whether grid should be visible
     */
    void setGridEnabled(bool enabled);

    /**
     * @brief Check if grid is enabled
     * @return true if grid is visible
     */
    [[nodiscard]] bool gridEnabled() const { return _data.grid.enabled; }

    /**
     * @brief Set the grid spacing
     * @param spacing Grid spacing in time units
     */
    void setGridSpacing(int spacing);

    /**
     * @brief Get the grid spacing
     * @return Grid spacing in time units
     */
    [[nodiscard]] int gridSpacing() const { return _data.grid.spacing; }

    /**
     * @brief Set the complete grid state
     * @param grid_state New grid state
     */
    void setGridState(DataViewerGridState const & grid_state);

    /**
     * @brief Get the complete grid state
     * @return Const reference to DataViewerGridState
     */
    [[nodiscard]] DataViewerGridState const & gridState() const { return _data.grid; }

    // ==================== UI Preferences ====================

    /**
     * @brief Set the zoom scaling mode
     * @param mode Zoom scaling mode enum value
     */
    void setZoomScalingMode(DataViewerZoomScalingMode mode);

    /**
     * @brief Get the zoom scaling mode
     * @return Current zoom scaling mode
     */
    [[nodiscard]] DataViewerZoomScalingMode zoomScalingMode() const { return _data.ui.zoom_scaling_mode; }

    /**
     * @brief Set whether the properties panel is collapsed
     * @param collapsed true if panel should be collapsed
     */
    void setPropertiesPanelCollapsed(bool collapsed);

    /**
     * @brief Check if properties panel is collapsed
     * @return true if panel is collapsed
     */
    [[nodiscard]] bool propertiesPanelCollapsed() const { return _data.ui.properties_panel_collapsed; }

    /**
     * @brief Set the complete UI preferences
     * @param prefs New UI preferences
     */
    void setUIPreferences(DataViewerUIPreferences const & prefs);

    /**
     * @brief Get the complete UI preferences
     * @return Const reference to DataViewerUIPreferences
     */
    [[nodiscard]] DataViewerUIPreferences const & uiPreferences() const { return _data.ui; }

    // ==================== Developer Mode ====================

    /**
     * @brief Toggle the developer diagnostics panel and on-canvas overlays
     * @param enabled true to enable developer mode
     */
    void setDeveloperMode(bool enabled);

    /**
     * @brief Check if developer mode is active
     * @return true if developer diagnostics are enabled
     */
    [[nodiscard]] bool developerMode() const { return _data.ui.developer_mode; }

    // ==================== Layout Config ====================

    /**
     * @brief Set the lane margin factor
     * @param factor Fraction of allocated lane height used for data (0..1]
     */
    void setMarginFactor(float factor);

    /**
     * @brief Get the lane margin factor
     * @return Current margin factor
     */
    [[nodiscard]] float marginFactor() const { return _data.layout.margin_factor; }

    /**
     * @brief Set the complete layout config
     * @param config New layout configuration
     */
    void setLayoutConfig(DataViewerLayoutConfig const & config);

    /**
     * @brief Get the complete layout config
     * @return Const reference to DataViewerLayoutConfig
     */
    [[nodiscard]] DataViewerLayoutConfig const & layoutConfig() const { return _data.layout; }

    // ==================== Interaction ====================

    /**
     * @brief Set the current interaction mode
     * @param mode New interaction mode
     */
    void setInteractionMode(DataViewerInteractionMode mode);

    /**
     * @brief Get the current interaction mode
     * @return Current interaction mode
     */
    [[nodiscard]] DataViewerInteractionMode interactionMode() const { return _data.interaction.mode; }

    // ==================== Group Scaling ====================

    /**
     * @brief Set group scaling state for a named group
     * @param group_name Group identifier (e.g., "voltage")
     * @param state Group scaling state
     */
    void setGroupScaling(std::string const & group_name, GroupScalingState const & state);

    /**
     * @brief Get group scaling state for a named group
     * @param group_name Group identifier
     * @return Pointer to group state, or nullptr if not found
     */
    [[nodiscard]] GroupScalingState const * getGroupScaling(std::string const & group_name) const;

    /**
     * @brief Get mutable group scaling state for a named group
     * @param group_name Group identifier
     * @return Pointer to group state, or nullptr if not found
     */
    [[nodiscard]] GroupScalingState * getGroupScalingMutable(std::string const & group_name);

    /**
     * @brief Check if unified scaling is enabled for a group
     * @param group_name Group identifier
     * @return true if unified scaling is enabled (default: true)
     */
    [[nodiscard]] bool isGroupUnifiedScaling(std::string const & group_name) const;

    /**
     * @brief Remove group scaling state
     * @param group_name Group identifier to remove
     */
    void removeGroupScaling(std::string const & group_name);

    /**
     * @brief Get the group scaling map (const)
     */
    [[nodiscard]] std::map<std::string, GroupScalingState> const & allGroupScaling() const {
        return _data.group_scaling;
    }

    // ==================== Mixed-Lane Overrides ====================

    /**
     * @brief Set or update a per-series lane placement override
     * @param series_key Data key for the series
     * @param override_data Lane placement override (normalized before storage)
     */
    void setSeriesLaneOverride(std::string const & series_key, SeriesLaneOverrideData const & override_data);

    /**
     * @brief Remove a per-series lane placement override
     * @param series_key Data key for the series
     */
    void clearSeriesLaneOverride(std::string const & series_key);

    /**
     * @brief Get per-series lane placement override
     * @param series_key Data key for the series
     * @return Pointer to override data, or nullptr when unset
     */
    [[nodiscard]] SeriesLaneOverrideData const * getSeriesLaneOverride(std::string const & series_key) const;

    /**
     * @brief Get all per-series lane placement overrides
     */
    [[nodiscard]] std::map<std::string, SeriesLaneOverrideData> const & allSeriesLaneOverrides() const {
        return _data.series_lane_overrides;
    }

    /**
     * @brief Set or update lane-level metadata override
     * @param lane_id Lane identifier
     * @param override_data Lane-level metadata (normalized before storage)
     */
    void setLaneOverride(std::string const & lane_id, LaneOverrideData const & override_data);

    /**
     * @brief Remove lane-level metadata override
     * @param lane_id Lane identifier
     */
    void clearLaneOverride(std::string const & lane_id);

    /**
     * @brief Get lane-level metadata override
     * @param lane_id Lane identifier
     * @return Pointer to lane override, or nullptr when unset
     */
    [[nodiscard]] LaneOverrideData const * getLaneOverride(std::string const & lane_id) const;

    /**
     * @brief Get all lane-level metadata overrides
     */
    [[nodiscard]] std::map<std::string, LaneOverrideData> const & allLaneOverrides() const {
        return _data.lane_overrides;
    }

    /**
     * @brief Replace all stackable ordering constraints
     * @param constraints Relative ordering constraints (normalized before storage)
     */
    void setOrderingConstraints(std::vector<StackableOrderingConstraintData> const & constraints);

    /**
     * @brief Add one stackable ordering constraint
     * @param constraint Relative ordering constraint to add
     */
    void addOrderingConstraint(StackableOrderingConstraintData const & constraint);

    /**
     * @brief Remove one stackable ordering constraint
     * @param constraint Relative ordering constraint to remove
     */
    void removeOrderingConstraint(StackableOrderingConstraintData const & constraint);

    /**
     * @brief Remove all stackable ordering constraints
     */
    void clearOrderingConstraints();

    /**
     * @brief Get all stackable ordering constraints
     */
    [[nodiscard]] std::vector<StackableOrderingConstraintData> const & orderingConstraints() const {
        return _data.ordering_constraints;
    }

    // ==================== Multi-Lane Axis ====================

    /**
     * @brief Get the multi-lane vertical axis state
     *
     * Lazily created on first access. Owned by this DataViewerState.
     *
     * @return Pointer to the MultiLaneVerticalAxisState
     */
    [[nodiscard]] MultiLaneVerticalAxisState * multiLaneAxisState();

signals:
    // === Consolidated Signals ===

    /**
     * @brief Emitted when any view state property changes
     * 
     * This includes time window, Y bounds, zoom, pan, and spacing changes.
     * For Y-axis-only changes (pan, zoom, bounds), the scene does NOT need
     * rebuilding — only the projection matrix needs updating.
     */
    void viewStateChanged();

    /**
     * @brief Emitted when the time window (x_min/x_max) changes
     * 
     * This signal indicates that the data range has changed and buffers
     * need rebuilding. Connect to this for scene-dirty operations.
     * Always emitted together with viewStateChanged().
     */
    void timeWindowChanged();

    /**
     * @brief Emitted when theme or colors change
     */
    void themeChanged();

    /**
     * @brief Emitted when grid settings change
     */
    void gridChanged();

    /**
     * @brief Emitted when UI preferences change
     */
    void uiPreferencesChanged();

    /**
     * @brief Emitted when developer mode is toggled
     * @param enabled true if developer mode is now active
     */
    void developerModeChanged(bool enabled);

    /**
     * @brief Emitted when interaction mode changes
     * @param mode New interaction mode
     */
    void interactionModeChanged(DataViewerInteractionMode mode);

    /**
     * @brief Emitted when layout configuration changes (lane sizing policy, height, gap)
     */
    void layoutConfigChanged();

    /**
     * @brief Emitted when group scaling state changes
     * @param group_name The group whose scaling changed
     */
    void groupScalingChanged(QString const & group_name);

    /**
     * @brief Emitted when a per-series lane override is changed or removed
     * @param series_key Data key of the series whose override changed
     */
    void seriesLaneOverrideChanged(QString const & series_key);

    /**
     * @brief Emitted when a lane-level override is changed or removed
     * @param lane_id Lane identifier whose override changed
     */
    void laneOverrideChanged(QString const & lane_id);

    /**
     * @brief Emitted when stackable ordering constraints are changed
     */
    void orderingConstraintsChanged();

    // === Series Options Signals (Forwarded from Registry) ===

    /**
     * @brief Emitted when series display options are modified
     * @param key The data key
     * @param type_name The type ("analog", "event", "interval")
     */
    void seriesOptionsChanged(QString const & key, QString const & type_name);

    /**
     * @brief Emitted when series display options are removed
     * @param key The data key
     * @param type_name The type
     */
    void seriesOptionsRemoved(QString const & key, QString const & type_name);

    /**
     * @brief Emitted when series visibility changes
     * @param key The data key
     * @param type_name The type
     * @param visible New visibility state
     */
    void seriesVisibilityChanged(QString const & key, QString const & type_name, bool visible);

private:
    DataViewerStateData _data;
    SeriesOptionsRegistry _series_options{&_data, this};
    std::unique_ptr<MultiLaneVerticalAxisState> _multi_lane_axis_state;

    /**
     * @brief Connect registry signals to state signals
     */
    void _connectRegistrySignals();
};

#endif// DATA_VIEWER_STATE_HPP
