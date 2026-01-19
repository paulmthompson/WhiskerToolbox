#ifndef MEDIA_WIDGET_STATE_HPP
#define MEDIA_WIDGET_STATE_HPP

/**
 * @file MediaWidgetState.hpp
 * @brief State class for Media_Widget
 * 
 * MediaWidgetState encapsulates all persistent state for a Media_Widget,
 * enabling serialization, undo/redo, and separation of concerns.
 * 
 * This serves as a template for migrating other widgets to the EditorState pattern.
 */

#include "EditorState/EditorState.hpp"

#include <rfl.hpp>
#include <rfl/json.hpp>

#include <map>
#include <string>
#include <vector>

/**
 * @brief Configuration for a single displayed feature in Media_Widget
 */
struct MediaFeatureConfig {
    bool enabled = false;       ///< Whether the feature is displayed
    std::string color = "";     ///< Hex color (empty = use default)
    float opacity = 1.0f;       ///< Opacity (0.0-1.0)
    int z_order = 0;            ///< Drawing order (higher = on top)
};

/**
 * @brief Serializable state data for Media_Widget
 * 
 * This struct is designed for reflect-cpp serialization.
 * All fields should have sensible defaults.
 */
struct MediaWidgetStateData {
    /// Features with their display configuration
    std::map<std::string, MediaFeatureConfig> features;
    
    /// Current zoom level
    double zoom_level = 1.0;
    
    /// Pan offset (viewport center in image coordinates)
    double pan_x = 0.0;
    double pan_y = 0.0;
    
    /// Whether user has manually adjusted zoom (vs auto-fit)
    bool user_zoom_active = false;
    
    /// Colormap settings for image processing
    std::string colormap_name = "gray";
    double colormap_min = 0.0;
    double colormap_max = 255.0;
    bool colormap_auto_range = true;
    
    /// Canvas/viewport settings
    int canvas_width = 800;
    int canvas_height = 600;
    
    /// Text overlay settings
    bool show_frame_number = true;
    bool show_timestamp = false;
};

/**
 * @brief Editor state for Media_Widget
 * 
 * MediaWidgetState manages all persistent state for a Media_Widget instance.
 * The widget observes this state and updates its display accordingly.
 * 
 * ## Usage
 * 
 * ```cpp
 * // Create state (typically via WorkspaceManager)
 * auto state = std::make_shared<MediaWidgetState>();
 * state->setDisplayName("Media Viewer 1");
 * 
 * // Create widget with state
 * auto* widget = new Media_Widget(state, data_manager, selection_context);
 * 
 * // State changes propagate to widget
 * state->setFeatureEnabled("whiskers", true);  // Widget updates
 * 
 * // Serialize for save
 * auto json = state->toJson();
 * ```
 * 
 * ## Signals
 * 
 * The state emits specific signals for each property change, allowing
 * widgets to update only the affected parts:
 * 
 * - `featureEnabledChanged(QString, bool)` - Feature enable state changed
 * - `featureColorChanged(QString, QString)` - Feature color changed
 * - `zoomChanged(double)` - Zoom level changed
 * - `panChanged(double, double)` - Pan position changed
 * - `colormapChanged()` - Colormap settings changed
 */
class MediaWidgetState : public EditorState {
    Q_OBJECT

public:
    explicit MediaWidgetState(QObject * parent = nullptr);
    ~MediaWidgetState() override = default;

    // === EditorState Interface ===
    
    [[nodiscard]] QString getTypeName() const override { return QStringLiteral("MediaWidget"); }
    [[nodiscard]] std::string toJson() const override;
    bool fromJson(std::string const & json) override;

    // === Feature Management ===
    
    /**
     * @brief Set whether a feature is displayed
     * @param key Data key in DataManager
     * @param enabled Whether to display
     */
    void setFeatureEnabled(std::string const & key, bool enabled);
    
    /**
     * @brief Check if a feature is enabled
     * @param key Data key
     * @return true if enabled
     */
    [[nodiscard]] bool isFeatureEnabled(std::string const & key) const;
    
    /**
     * @brief Get all enabled feature keys
     * @return Vector of enabled keys
     */
    [[nodiscard]] std::vector<std::string> enabledFeatures() const;
    
    /**
     * @brief Set feature display color
     * @param key Data key
     * @param hex_color Color in "#RRGGBB" format
     */
    void setFeatureColor(std::string const & key, std::string const & hex_color);
    
    /**
     * @brief Get feature display color
     * @param key Data key
     * @return Color in "#RRGGBB" format, or empty if not set
     */
    [[nodiscard]] std::string getFeatureColor(std::string const & key) const;
    
    /**
     * @brief Set feature opacity
     * @param key Data key
     * @param opacity Opacity (0.0-1.0)
     */
    void setFeatureOpacity(std::string const & key, float opacity);
    
    /**
     * @brief Get feature opacity
     * @param key Data key
     * @return Opacity (0.0-1.0), defaults to 1.0 if not set
     */
    [[nodiscard]] float getFeatureOpacity(std::string const & key) const;
    
    /**
     * @brief Remove feature from state (when data is deleted)
     * @param key Data key to remove
     */
    void removeFeature(std::string const & key);

    // === Viewport ===
    
    /**
     * @brief Set zoom level
     * @param zoom Zoom factor (1.0 = 100%)
     */
    void setZoom(double zoom);
    
    /**
     * @brief Get current zoom level
     * @return Zoom factor
     */
    [[nodiscard]] double getZoom() const { return _data.zoom_level; }
    
    /**
     * @brief Set pan offset
     * @param x X offset in image coordinates
     * @param y Y offset in image coordinates
     */
    void setPan(double x, double y);
    
    /**
     * @brief Get pan X offset
     * @return X offset
     */
    [[nodiscard]] double getPanX() const { return _data.pan_x; }
    
    /**
     * @brief Get pan Y offset
     * @return Y offset
     */
    [[nodiscard]] double getPanY() const { return _data.pan_y; }
    
    /**
     * @brief Set whether user has manually adjusted zoom
     * @param active true if user zoom is active
     */
    void setUserZoomActive(bool active);
    
    /**
     * @brief Check if user zoom is active
     * @return true if user has manually zoomed
     */
    [[nodiscard]] bool isUserZoomActive() const { return _data.user_zoom_active; }
    
    /**
     * @brief Reset viewport to auto-fit
     */
    void resetViewport();

    // === Colormap ===
    
    /**
     * @brief Set colormap name
     * @param name Colormap name (e.g., "gray", "jet", "viridis")
     */
    void setColormapName(std::string const & name);
    
    /**
     * @brief Get colormap name
     * @return Current colormap name
     */
    [[nodiscard]] std::string getColormapName() const { return _data.colormap_name; }
    
    /**
     * @brief Set colormap range
     * @param min Minimum value
     * @param max Maximum value
     */
    void setColormapRange(double min, double max);
    
    /**
     * @brief Get colormap minimum
     * @return Minimum value
     */
    [[nodiscard]] double getColormapMin() const { return _data.colormap_min; }
    
    /**
     * @brief Get colormap maximum
     * @return Maximum value
     */
    [[nodiscard]] double getColormapMax() const { return _data.colormap_max; }
    
    /**
     * @brief Set auto-range mode for colormap
     * @param auto_range true to auto-compute range from data
     */
    void setColormapAutoRange(bool auto_range);
    
    /**
     * @brief Check if colormap is in auto-range mode
     * @return true if auto-ranging
     */
    [[nodiscard]] bool isColormapAutoRange() const { return _data.colormap_auto_range; }

    // === Text Overlay ===
    
    /**
     * @brief Set whether to show frame number overlay
     * @param show true to show
     */
    void setShowFrameNumber(bool show);
    
    /**
     * @brief Check if frame number is shown
     * @return true if shown
     */
    [[nodiscard]] bool showFrameNumber() const { return _data.show_frame_number; }
    
    /**
     * @brief Set whether to show timestamp overlay
     * @param show true to show
     */
    void setShowTimestamp(bool show);
    
    /**
     * @brief Check if timestamp is shown
     * @return true if shown
     */
    [[nodiscard]] bool showTimestamp() const { return _data.show_timestamp; }

signals:
    /**
     * @brief Emitted when a feature's enabled state changes
     * @param key Data key
     * @param enabled New enabled state
     */
    void featureEnabledChanged(QString const & key, bool enabled);
    
    /**
     * @brief Emitted when a feature's color changes
     * @param key Data key
     * @param color New color in hex format
     */
    void featureColorChanged(QString const & key, QString const & color);
    
    /**
     * @brief Emitted when a feature's opacity changes
     * @param key Data key
     * @param opacity New opacity
     */
    void featureOpacityChanged(QString const & key, float opacity);
    
    /**
     * @brief Emitted when a feature is removed
     * @param key Data key that was removed
     */
    void featureRemoved(QString const & key);
    
    /**
     * @brief Emitted when zoom level changes
     * @param zoom New zoom level
     */
    void zoomChanged(double zoom);
    
    /**
     * @brief Emitted when pan position changes
     * @param x New X offset
     * @param y New Y offset
     */
    void panChanged(double x, double y);
    
    /**
     * @brief Emitted when viewport is reset
     */
    void viewportReset();
    
    /**
     * @brief Emitted when colormap settings change
     */
    void colormapChanged();
    
    /**
     * @brief Emitted when text overlay settings change
     */
    void textOverlayChanged();

private:
    MediaWidgetStateData _data;
    
    /// Get or create feature config (for setters)
    MediaFeatureConfig & getOrCreateConfig(std::string const & key);
};

#endif // MEDIA_WIDGET_STATE_HPP
