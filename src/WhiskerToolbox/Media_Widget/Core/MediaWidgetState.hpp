#ifndef MEDIA_WIDGET_STATE_HPP
#define MEDIA_WIDGET_STATE_HPP

/**
 * @file MediaWidgetState.hpp
 * @brief State class for Media_Widget
 * 
 * MediaWidgetState manages the serializable state for the Media_Widget,
 * enabling workspace save/restore and inter-widget communication via SelectionContext.
 * 
 * ## Current Implementation (Phase 3 - Complete)
 * 
 * This implementation uses MediaWidgetStateData which contains:
 * - Displayed data key (primary media being viewed)
 * - Viewport state (zoom, pan, canvas size)
 * - Per-feature display options for all data types
 * - Interaction preferences (line tools, mask brush, point selection)
 * - Text overlays
 * - Active tool modes
 * 
 * All state properties have typed accessors and Qt signals for change notification.
 * This enables properties panels and other widgets to observe and modify state.
 * 
 * ## Usage
 * 
 * ```cpp
 * // Create state (typically done by WorkspaceManager)
 * auto state = std::make_shared<MediaWidgetState>();
 * 
 * // === Viewport ===
 * state->setZoom(2.0);
 * connect(state.get(), &MediaWidgetState::zoomChanged,
 *         this, &MyWidget::onZoomChanged);
 * 
 * // === Display Options ===
 * LineDisplayOptions opts;
 * opts.hex_color() = "#ff0000";
 * opts.line_thickness = 3;
 * state->setLineOptions("whisker_1", opts);
 * 
 * // === Feature Visibility ===
 * state->setFeatureEnabled("whisker_1", "line", true);
 * 
 * // === Text Overlays ===
 * TextOverlayData overlay;
 * overlay.text = "Frame: 100";
 * int id = state->addTextOverlay(overlay);
 * 
 * // === Serialization ===
 * std::string json = state->toJson();
 * state->fromJson(json);
 * ```
 * 
 * ## Integration with SelectionContext
 * 
 * The widget should respond to SelectionContext changes:
 * 
 * ```cpp
 * connect(_selection_context, &SelectionContext::selectionChanged,
 *         this, [this](SelectionSource const& source) {
 *     if (source.editor_instance_id == _state->getInstanceId()) {
 *         return; // Ignore our own changes
 *     }
 *     // Respond to external selection
 *     QString selected = _selection_context->primarySelectedData();
 *     highlightFeatureIfPresent(selected);
 * });
 * ```
 * 
 * @see EditorState for base class documentation
 * @see MediaWidgetStateData for the complete state structure
 * @see SelectionContext for inter-widget communication
 */

#include "EditorState/EditorState.hpp"
#include "EditorState/StrongTypes.hpp"  // Must be before any TimePosition usage in signals
#include "MediaWidgetStateData.hpp"
#include "DisplayOptionsRegistry.hpp"

#include "TimeFrame/TimeFrame.hpp"

#include <rfl.hpp>
#include <rfl/json.hpp>

#include <QImage>
#include <QStringList>

#include <string>
#include <utility>
#include <variant>

/**
 * @brief Enumeration of display option types for unified API
 */
enum class DisplayType {
    Line,
    Mask,
    Point,
    Tensor,
    Interval,
    Media
};

/**
 * @brief Variant type holding any display options type
 * 
 * Used with setOptions() for type-safe, unified option setting.
 * The type is inferred from the variant alternative passed.
 */
using DisplayOptionsVariant = std::variant<
    LineDisplayOptions,
    MaskDisplayOptions,
    PointDisplayOptions,
    TensorDisplayOptions,
    DigitalIntervalDisplayOptions,
    MediaDisplayOptions
>;

/**
 * @brief State class for Media_Widget
 * 
 * MediaWidgetState is the Qt wrapper around MediaWidgetStateData that provides
 * typed accessors and Qt signals for all state properties.
 * 
 * ## Signal Categories
 * 
 * - **Viewport**: zoomChanged, panChanged, canvasSizeChanged
 * - **Features**: featureEnabledChanged, displayOptionsChanged
 * - **Interaction**: linePrefsChanged, maskPrefsChanged, pointPrefsChanged
 * - **Text Overlays**: textOverlayAdded, textOverlayRemoved, textOverlayUpdated
 * - **Tool Modes**: activeLineModeChanged, activeMaskModeChanged, activePointModeChanged
 * 
 * ## Thread Safety
 * 
 * This class is NOT thread-safe. All access should be from the main/GUI thread.
 */
class MediaWidgetState : public EditorState {
    Q_OBJECT

public:
    /**
     * @brief Construct a new MediaWidgetState
     * @param parent Parent QObject (typically nullptr, managed by WorkspaceManager)
     */
    explicit MediaWidgetState(QObject * parent = nullptr);

    ~MediaWidgetState() override = default;

    // === Type Identification ===

    /**
     * @brief Get the type name for this state
     * @return "MediaWidget"
     */
    [[nodiscard]] QString getTypeName() const override { return QStringLiteral("MediaWidget"); }

    /**
     * @brief Get the display name for UI
     * @return User-visible name (default: "Media Viewer")
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
     * @return Const reference to MediaWidgetStateData
     */
    [[nodiscard]] MediaWidgetStateData const & data() const { return _data; }

    // === Display Options Registry (NEW - Phase 4A) ===

    /**
     * @brief Get the display options registry for generic access
     * 
     * The registry provides a unified API for all display option types:
     * 
     * ```cpp
     * // Set options (type inferred)
     * LineDisplayOptions opts;
     * opts.line_thickness = 3;
     * state->displayOptions().set("whisker_1", opts);
     * 
     * // Get options (type explicit)
     * auto* opts = state->displayOptions().get<LineDisplayOptions>("whisker_1");
     * 
     * // Get enabled keys
     * QStringList enabled = state->displayOptions().enabledKeys<LineDisplayOptions>();
     * ```
     * 
     * @return Reference to DisplayOptionsRegistry
     */
    [[nodiscard]] DisplayOptionsRegistry & displayOptions() { return _display_options; }
    [[nodiscard]] DisplayOptionsRegistry const & displayOptions() const { return _display_options; }

    // === Displayed Data Key ===

    /**
     * @brief Set the displayed data key
     * 
     * This represents the primary data being displayed in the Media_Widget.
     * For image/video data, this is typically the media source.
     * 
     * @param key Data key to display (empty string to clear)
     */
    void setDisplayedDataKey(QString const & key);

    /**
     * @brief Get the displayed data key
     * @return Currently displayed data key, or empty string if none
     */
    [[nodiscard]] QString displayedDataKey() const;

    // === Viewport State ===

    /**
     * @brief Set the zoom level
     * @param zoom Zoom factor (1.0 = no zoom)
     */
    void setZoom(double zoom);

    /**
     * @brief Get the zoom level
     * @return Current zoom factor
     */
    [[nodiscard]] double zoom() const { return _data.viewport.zoom; }

    /**
     * @brief Set the pan offset
     * @param x Horizontal pan offset in pixels
     * @param y Vertical pan offset in pixels
     */
    void setPan(double x, double y);

    /**
     * @brief Get the pan offset
     * @return Pair of (x, y) pan offset in pixels
     */
    [[nodiscard]] std::pair<double, double> pan() const;

    /**
     * @brief Set the canvas size
     * @param width Canvas width in pixels
     * @param height Canvas height in pixels
     */
    void setCanvasSize(int width, int height);

    /**
     * @brief Get the canvas size
     * @return Pair of (width, height) in pixels
     */
    [[nodiscard]] std::pair<int, int> canvasSize() const;

    /**
     * @brief Get the complete viewport state
     * @return Const reference to ViewportState
     */
    [[nodiscard]] ViewportState const & viewport() const { return _data.viewport; }

    /**
     * @brief Set the complete viewport state
     * @param viewport New viewport state
     */
    void setViewport(ViewportState const & viewport);

    // === Feature Management ===

    /**
     * @brief Enable or disable a feature
     * 
     * This sets the is_visible flag in the corresponding display options.
     * If options don't exist for this key, they will be created with defaults.
     * 
     * @param data_key The data key (e.g., "whisker_1")
     * @param data_type The type ("line", "mask", "point", "tensor", "interval", "media")
     * @param enabled Whether the feature should be visible
     */
    void setFeatureEnabled(QString const & data_key, QString const & data_type, bool enabled);

    /**
     * @brief Check if a feature is enabled
     * @param data_key The data key
     * @param data_type The type
     * @return true if the feature exists and is visible
     */
    [[nodiscard]] bool isFeatureEnabled(QString const & data_key, QString const & data_type) const;

    /**
     * @brief Get list of enabled features for a data type
     * @param data_type The type ("line", "mask", etc.)
     * @return List of data keys that are enabled
     */
    [[nodiscard]] QStringList enabledFeatures(QString const & data_type) const;

    // === Interaction Preferences ===

    /**
     * @brief Get line interaction preferences
     * @return Const reference to LineInteractionPrefs
     */
    [[nodiscard]] LineInteractionPrefs const & linePrefs() const { return _data.line_prefs; }

    /**
     * @brief Set line interaction preferences
     * @param prefs The new preferences
     */
    void setLinePrefs(LineInteractionPrefs const & prefs);

    /**
     * @brief Get mask interaction preferences
     * @return Const reference to MaskInteractionPrefs
     */
    [[nodiscard]] MaskInteractionPrefs const & maskPrefs() const { return _data.mask_prefs; }

    /**
     * @brief Set mask interaction preferences
     * @param prefs The new preferences
     */
    void setMaskPrefs(MaskInteractionPrefs const & prefs);

    /**
     * @brief Get point interaction preferences
     * @return Const reference to PointInteractionPrefs
     */
    [[nodiscard]] PointInteractionPrefs const & pointPrefs() const { return _data.point_prefs; }

    /**
     * @brief Set point interaction preferences
     * @param prefs The new preferences
     */
    void setPointPrefs(PointInteractionPrefs const & prefs);

    // === Text Overlays ===

    /**
     * @brief Get all text overlays
     * @return Const reference to text overlay vector
     */
    [[nodiscard]] std::vector<TextOverlayData> const & textOverlays() const { return _data.text_overlays; }

    /**
     * @brief Add a text overlay
     * 
     * The overlay's ID will be assigned automatically.
     * 
     * @param overlay The overlay data (id field will be overwritten)
     * @return The assigned overlay ID
     */
    int addTextOverlay(TextOverlayData overlay);

    /**
     * @brief Remove a text overlay by ID
     * @param overlay_id The overlay ID to remove
     * @return true if the overlay was found and removed
     */
    bool removeTextOverlay(int overlay_id);

    /**
     * @brief Update an existing text overlay
     * @param overlay_id The overlay ID to update
     * @param overlay The new overlay data (id field preserved)
     * @return true if the overlay was found and updated
     */
    bool updateTextOverlay(int overlay_id, TextOverlayData const & overlay);

    /**
     * @brief Clear all text overlays
     */
    void clearTextOverlays();

    /**
     * @brief Get a text overlay by ID
     * @param overlay_id The overlay ID
     * @return Pointer to overlay, or nullptr if not found
     */
    [[nodiscard]] TextOverlayData const * getTextOverlay(int overlay_id) const;

    // === Active Tool State ===

    /**
     * @brief Set the active line tool mode
     * @param mode The tool mode
     */
    void setActiveLineMode(LineToolMode mode);

    /**
     * @brief Get the active line tool mode
     * @return Current line tool mode
     */
    [[nodiscard]] LineToolMode activeLineMode() const { return _data.active_line_mode; }

    /**
     * @brief Set the active mask tool mode
     * @param mode The tool mode
     */
    void setActiveMaskMode(MaskToolMode mode);

    /**
     * @brief Get the active mask tool mode
     * @return Current mask tool mode
     */
    [[nodiscard]] MaskToolMode activeMaskMode() const { return _data.active_mask_mode; }

    /**
     * @brief Set the active point tool mode
     * @param mode The tool mode
     */
    void setActivePointMode(PointToolMode mode);

    /**
     * @brief Get the active point tool mode
     * @return Current point tool mode
     */
    [[nodiscard]] PointToolMode activePointMode() const { return _data.active_point_mode; }

signals:
    // === Primary Display ===

    /**
     * @brief Emitted when the displayed data key changes
     * @param key The new displayed data key
     */
    void displayedDataKeyChanged(QString const & key);

    // === Viewport Signals ===

    /**
     * @brief Emitted when zoom changes
     * @param zoom New zoom factor
     */
    void zoomChanged(double zoom);

    /**
     * @brief Emitted when pan position changes
     * @param x New horizontal pan offset
     * @param y New vertical pan offset
     */
    void panChanged(double x, double y);

    /**
     * @brief Emitted when canvas size changes
     * @param width New canvas width
     * @param height New canvas height
     */
    void canvasSizeChanged(int width, int height);

    /**
     * @brief Emitted when any viewport property changes
     */
    void viewportChanged();

    // === Feature Signals ===

    /**
     * @brief Emitted when a feature's enabled state changes
     * @param data_key The data key
     * @param data_type The type ("line", "mask", etc.)
     * @param enabled New enabled state
     */
    void featureEnabledChanged(QString const & data_key, QString const & data_type, bool enabled);

    /**
     * @brief Emitted when display options for a feature change
     * @param data_key The data key
     * @param data_type The type ("line", "mask", etc.)
     */
    void displayOptionsChanged(QString const & data_key, QString const & data_type);

    /**
     * @brief Emitted when display options for a feature are removed
     * @param data_key The data key
     * @param data_type The type
     */
    void displayOptionsRemoved(QString const & data_key, QString const & data_type);

    // === Interaction Preference Signals ===

    /**
     * @brief Emitted when any interaction preferences change (consolidated)
     * @param category "line", "mask", or "point"
     */
    void interactionPrefsChanged(QString const & category);

    // === Text Overlay Signals ===

    /**
     * @brief Emitted when any text overlay operation occurs (consolidated)
     * 
     * This signal is emitted after any add/remove/update/clear operation.
     */
    void textOverlaysChanged();

    // === Tool Mode Signals ===

    /**
     * @brief Emitted when any active tool mode changes (consolidated)
     * @param category "line", "mask", or "point"
     */
    void toolModesChanged(QString const & category);

    // === Canvas Image (Transient - for video export) ===

    /**
     * @brief Emitted when the rendered canvas image changes
     * @param image The new canvas image
     * 
     * This signal is emitted by Media_Window after rendering completes.
     * Export_Video_Widget connects to this to capture frames.
     */
    void canvasImageChanged(QImage const & image);

public:
    // === Canvas Image (Transient - not serialized) ===

    /**
     * @brief Set the current canvas image
     * 
     * Called by Media_Window after rendering to update the transient canvas image.
     * This is NOT serialized - it's only for inter-widget communication.
     * 
     * @param image The rendered canvas image
     */
    void setCanvasImage(QImage const & image);

    /**
     * @brief Get the current canvas image
     * @return The most recently rendered canvas image (may be null)
     */
    [[nodiscard]] QImage const & canvasImage() const { return _canvas_image; }

private:
    MediaWidgetStateData _data;
    DisplayOptionsRegistry _display_options{&_data, this};

    /// Transient canvas image (not serialized) - updated by Media_Window
    QImage _canvas_image;

    // Helper to forward registry signals to state signals
    void _connectRegistrySignals();


    void removeMaskOptions(QString const & key);
    void removeLineOptions(QString const & key);
    void removePointOptions(QString const & key);
    void removeTensorOptions(QString const & key);
    void removeIntervalOptions(QString const & key);
    void removeMediaOptions(QString const & key);


    void setLineOptions(QString const & key, LineDisplayOptions const & options);
    void setMaskOptions(QString const & key, MaskDisplayOptions const & options);
    void setPointOptions(QString const & key, PointDisplayOptions const & options);
    void setTensorOptions(QString const & key, TensorDisplayOptions const & options);
    void setIntervalOptions(QString const & key, DigitalIntervalDisplayOptions const & options);
    void setMediaOptions(QString const & key, MediaDisplayOptions const & options);
};

#endif // MEDIA_WIDGET_STATE_HPP
