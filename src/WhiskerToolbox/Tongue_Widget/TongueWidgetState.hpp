#ifndef TONGUE_WIDGET_STATE_HPP
#define TONGUE_WIDGET_STATE_HPP

/**
 * @file TongueWidgetState.hpp
 * @brief State class for Tongue_Widget
 * 
 * TongueWidgetState manages the serializable state for the Tongue_Widget,
 * enabling workspace save/restore and integration with the EditorRegistry system.
 * 
 * State tracked:
 * - Processed frames list
 * 
 * @see EditorState for base class documentation
 * @see Tongue_Widget for the widget implementation
 */

#include "EditorState/EditorState.hpp"
#include "EditorState/StrongTypes.hpp"  // Must be before any TimePosition usage in signals
#include "TimeFrame/TimeFrame.hpp"

#include <rfl.hpp>
#include <rfl/json.hpp>

#include <string>
#include <vector>

/**
 * @brief Serializable data structure for TongueWidgetState
 * 
 * This struct is designed for reflect-cpp serialization.
 * All members should be default-constructible and serializable.
 */
struct TongueWidgetStateData {
    std::vector<int> processed_frames;     ///< Frames that have been processed with GrabCut
    std::string instance_id;               ///< Unique instance ID (preserved across serialization)
    std::string display_name = "Tongue Tracking";  ///< User-visible name
};

/**
 * @brief State class for Tongue_Widget
 * 
 * TongueWidgetState is a single-instance widget state that manages
 * the tongue tracking configuration and processed frame history.
 * 
 * ## Usage
 * 
 * ```cpp
 * // Create state (typically done by EditorCreationController)
 * auto state = std::make_shared<TongueWidgetState>();
 * registry->registerState(state);
 * 
 * // Serialize for workspace save
 * std::string json = state->toJson();
 * ```
 */
class TongueWidgetState : public EditorState {
    Q_OBJECT

public:
    /**
     * @brief Construct a new TongueWidgetState
     * @param parent Parent QObject (typically nullptr, managed by WorkspaceManager)
     */
    explicit TongueWidgetState(QObject * parent = nullptr);

    ~TongueWidgetState() override = default;

    // === Type Identification ===

    /**
     * @brief Get the type name for this state
     * @return "TongueWidget"
     */
    [[nodiscard]] QString getTypeName() const override { return QStringLiteral("TongueWidget"); }

    /**
     * @brief Get the display name for UI
     * @return User-visible name (default: "Tongue Tracking")
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

    // === State Properties ===

    /**
     * @brief Add a processed frame to the history
     * @param frame Frame number that was processed
     */
    void addProcessedFrame(int frame);

    /**
     * @brief Get all processed frames
     * @return Vector of processed frame numbers
     */
    [[nodiscard]] std::vector<int> const & processedFrames() const;

    /**
     * @brief Clear all processed frames
     */
    void clearProcessedFrames();

signals:
    /**
     * @brief Emitted when a frame is processed
     * @param frame The processed frame number
     */
    void frameProcessed(int frame);

    /**
     * @brief Emitted when processed frames are cleared
     */
    void processedFramesCleared();

private:
    TongueWidgetStateData _data;
};

#endif // TONGUE_WIDGET_STATE_HPP
