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
 * | View State | Time window, Y bounds, zoom | time_start, time_end, global_zoom |
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
 * state->setGlobalZoom(1.5);
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

#include "EditorState/EditorState.hpp"
#include "DataViewerStateData.hpp"
#include "SeriesOptionsRegistry.hpp"

#include <rfl.hpp>
#include <rfl/json.hpp>

#include <QStringList>

#include <cstdint>
#include <string>
#include <utility>

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

    ~DataViewerState() override = default;

    // === Type Identification ===

    /**
     * @brief Get the type name for this state
     * @return "DataViewer"
     */
    [[nodiscard]] QString getTypeName() const override { return QStringLiteral("DataViewer"); }

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
    [[nodiscard]] float verticalPanOffset() const { return _data.view.vertical_pan_offset; }

    /**
     * @brief Set the global zoom level
     * @param zoom Zoom/amplitude scale factor
     */
    void setGlobalZoom(float zoom);

    /**
     * @brief Get the global zoom level
     * @return Current global zoom factor
     */
    [[nodiscard]] float globalZoom() const { return _data.view.global_zoom; }

    /**
     * @brief Set the global vertical scale
     * @param scale Vertical scale factor
     */
    void setGlobalVerticalScale(float scale);

    /**
     * @brief Get the global vertical scale
     * @return Current global vertical scale
     */
    [[nodiscard]] float globalVerticalScale() const { return _data.view.global_vertical_scale; }

    /**
     * @brief Set the Y-spacing between series
     * @param spacing Spacing factor (0.0 to 1.0 typical)
     */
    void setYSpacing(float spacing);

    /**
     * @brief Get the Y-spacing between series
     * @return Current Y-spacing factor
     */
    [[nodiscard]] float ySpacing() const { return _data.view.y_spacing; }

    /**
     * @brief Set the complete view state
     * @param view New view state
     */
    void setViewState(DataViewerViewState const & view);

    /**
     * @brief Get the complete view state
     * @return Const reference to DataViewerViewState
     */
    [[nodiscard]] DataViewerViewState const & viewState() const { return _data.view; }

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

signals:
    // === Consolidated Signals ===

    /**
     * @brief Emitted when any view state property changes
     * 
     * This includes time window, Y bounds, zoom, pan, and spacing changes.
     */
    void viewStateChanged();

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
     * @brief Emitted when interaction mode changes
     * @param mode New interaction mode
     */
    void interactionModeChanged(DataViewerInteractionMode mode);

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

    /**
     * @brief Connect registry signals to state signals
     */
    void _connectRegistrySignals();
};

#endif // DATA_VIEWER_STATE_HPP
