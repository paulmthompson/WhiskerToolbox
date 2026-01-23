#ifndef TIMESCROLLBAR_STATE_HPP
#define TIMESCROLLBAR_STATE_HPP

/**
 * @file TimeScrollBarState.hpp
 * @brief State class for TimeScrollBar
 * 
 * TimeScrollBarState manages the serializable state for the TimeScrollBar widget,
 * enabling workspace save/restore and integration with the EditorRegistry system.
 * 
 * State tracked:
 * - Play speed multiplier
 * - Frame jump value
 * - Play mode (playing/paused)
 * 
 * The TimeScrollBar is a singleton widget that exists in Zone::Bottom and provides
 * global timeline control for the application.
 * 
 * @see EditorState for base class documentation
 * @see TimeScrollBar for the widget implementation
 */

#include "EditorState/EditorState.hpp"

#include <rfl.hpp>
#include <rfl/json.hpp>

#include <string>

/**
 * @brief Serializable data structure for TimeScrollBarState
 * 
 * This struct is designed for reflect-cpp serialization.
 * All members should be default-constructible and serializable.
 */
struct TimeScrollBarStateData {
    std::string instance_id;                          ///< Unique instance ID (preserved across serialization)
    std::string display_name = "Timeline";            ///< User-visible name
    
    // Playback parameters
    int play_speed = 1;                               ///< Play speed multiplier (1x, 2x, etc.)
    int frame_jump = 10;                              ///< Frame jump value for keyboard shortcuts
    bool is_playing = false;                          ///< Whether video is currently playing
};

/**
 * @brief State class for TimeScrollBar
 * 
 * TimeScrollBarState is a single-instance widget state that manages
 * the timeline control configuration.
 * 
 * ## Usage
 * 
 * ```cpp
 * // Create state (typically done by EditorCreationController)
 * auto state = std::make_shared<TimeScrollBarState>();
 * registry->registerState(state);
 * 
 * // Modify settings
 * state->setPlaySpeed(2);
 * state->setFrameJump(25);
 * 
 * // Serialize for workspace save
 * std::string json = state->toJson();
 * ```
 */
class TimeScrollBarState : public EditorState {
    Q_OBJECT

public:
    /**
     * @brief Construct a new TimeScrollBarState
     * @param parent Parent QObject (typically nullptr, managed by WorkspaceManager)
     */
    explicit TimeScrollBarState(QObject * parent = nullptr);

    ~TimeScrollBarState() override = default;

    // === Type Identification ===

    /**
     * @brief Get the type name for this state
     * @return "TimeScrollBar"
     */
    [[nodiscard]] QString getTypeName() const override { return QStringLiteral("TimeScrollBar"); }

    /**
     * @brief Get the display name for UI
     * @return User-visible name (default: "Timeline")
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

    // === State Properties - Getters ===

    [[nodiscard]] int playSpeed() const;
    [[nodiscard]] int frameJump() const;
    [[nodiscard]] bool isPlaying() const;

    // === State Properties - Setters ===

    void setPlaySpeed(int speed);
    void setFrameJump(int jump);
    void setIsPlaying(bool playing);

signals:
    /**
     * @brief Emitted when play speed changes
     */
    void playSpeedChanged(int speed);

    /**
     * @brief Emitted when frame jump value changes
     */
    void frameJumpChanged(int jump);

    /**
     * @brief Emitted when play state changes
     */
    void isPlayingChanged(bool playing);

private:
    TimeScrollBarStateData _data;
};

#endif // TIMESCROLLBAR_STATE_HPP
