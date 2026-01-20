#ifndef MEDIA_WIDGET_STATE_HPP
#define MEDIA_WIDGET_STATE_HPP

/**
 * @file MediaWidgetState.hpp
 * @brief State class for Media_Widget
 * 
 * MediaWidgetState manages the serializable state for the Media_Widget,
 * enabling workspace save/restore and inter-widget communication via SelectionContext.
 * 
 * This is a minimal implementation for Phase 2.1 that tracks:
 * - Displayed data key (primary media being viewed)
 * 
 * Future phases will add:
 * - Enabled features and their colors
 * - Zoom level and pan position
 * - Canvas size
 * - Per-feature display options
 * 
 * @see EditorState for base class documentation
 * @see SelectionContext for inter-widget communication
 */

#include "EditorState/EditorState.hpp"

#include <rfl.hpp>
#include <rfl/json.hpp>

#include <string>

/**
 * @brief Serializable data structure for MediaWidgetState
 * 
 * This struct is designed for reflect-cpp serialization.
 * All members should be default-constructible and serializable.
 */
struct MediaWidgetStateData {
    std::string displayed_data_key;  ///< Primary data key being displayed
    std::string instance_id;         ///< Unique instance ID (preserved across serialization)
    std::string display_name = "Media Viewer";  ///< User-visible name
};

/**
 * @brief State class for Media_Widget
 * 
 * MediaWidgetState is a minimal EditorState subclass that tracks
 * the primary displayed data key in the Media_Widget.
 * 
 * ## Usage
 * 
 * ```cpp
 * // Create state (typically done by WorkspaceManager)
 * auto state = std::make_shared<MediaWidgetState>();
 * 
 * // Connect to display changes
 * connect(state.get(), &MediaWidgetState::displayedDataKeyChanged,
 *         this, &MyWidget::onDisplayChanged);
 * 
 * // Update display
 * state->setDisplayedDataKey("video_data");
 * 
 * // Serialize for workspace save
 * std::string json = state->toJson();
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

    // === State Properties ===

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

signals:
    /**
     * @brief Emitted when the displayed data key changes
     * @param key The new displayed data key
     */
    void displayedDataKeyChanged(QString const & key);

private:
    MediaWidgetStateData _data;
};

#endif // MEDIA_WIDGET_STATE_HPP
