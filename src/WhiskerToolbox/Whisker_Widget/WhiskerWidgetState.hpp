#ifndef WHISKER_WIDGET_STATE_HPP
#define WHISKER_WIDGET_STATE_HPP

/**
 * @file WhiskerWidgetState.hpp
 * @brief State class for Whisker_Widget
 * 
 * WhiskerWidgetState manages the serializable state for the Whisker_Widget,
 * enabling workspace save/restore and integration with the EditorRegistry system.
 * 
 * State tracked:
 * - Face orientation (top, bottom, left, right)
 * - Number of whiskers to track
 * - Length threshold
 * - Clip length
 * - Linking tolerance
 * - Whisker pad key and position
 * - Mask mode settings
 * 
 * @see EditorState for base class documentation
 * @see Whisker_Widget for the widget implementation
 */

#include "EditorState/EditorState.hpp"
#include "EditorState/StrongTypes.hpp"  // Must be before any TimePosition usage in signals

#include "TimeFrame/TimeFrame.hpp"

#include <rfl.hpp>
#include <rfl/json.hpp>

#include <string>

/**
 * @brief Enumeration for face orientation
 */
enum class FaceOrientation {
    Top = 0,
    Bottom = 1,
    Left = 2,
    Right = 3
};

/**
 * @brief Serializable data structure for WhiskerWidgetState
 * 
 * This struct is designed for reflect-cpp serialization.
 * All members should be default-constructible and serializable.
 */
struct WhiskerWidgetStateData {
    std::string instance_id;                          ///< Unique instance ID (preserved across serialization)
    std::string display_name = "Whisker Tracking";    ///< User-visible name
    
    // Tracking parameters
    int face_orientation = 0;                         ///< Face orientation (0=Top, 1=Bottom, 2=Left, 3=Right)
    int num_whiskers_to_track = 0;                    ///< Number of whiskers to track
    double length_threshold = 30.0;                   ///< Minimum whisker length threshold
    int clip_length = 0;                              ///< Clip length for whisker processing
    float linking_tolerance = 20.0f;                  ///< Tolerance for linking whiskers across frames
    
    // Whisker pad settings
    std::string whisker_pad_key;                      ///< Current selected PointData key for whisker pad
    float whisker_pad_x = 0.0f;                       ///< Whisker pad X position
    float whisker_pad_y = 0.0f;                       ///< Whisker pad Y position
    
    // Mask mode settings
    bool use_mask_mode = false;                       ///< Whether mask mode is enabled
    std::string selected_mask_key;                    ///< Selected mask key for mask mode
    
    // Current whisker selection
    int current_whisker = 0;                          ///< Currently selected whisker index
    
    // Auto DL mode
    bool auto_dl = false;                             ///< Whether auto deep learning mode is enabled
};

/**
 * @brief State class for Whisker_Widget
 * 
 * WhiskerWidgetState is a single-instance widget state that manages
 * the whisker tracking configuration.
 * 
 * ## Usage
 * 
 * ```cpp
 * // Create state (typically done by EditorCreationController)
 * auto state = std::make_shared<WhiskerWidgetState>();
 * registry->registerState(state);
 * 
 * // Modify settings
 * state->setNumWhiskersToTrack(3);
 * state->setFaceOrientation(FaceOrientation::Left);
 * 
 * // Serialize for workspace save
 * std::string json = state->toJson();
 * ```
 */
class WhiskerWidgetState : public EditorState {
    Q_OBJECT

public:
    /**
     * @brief Construct a new WhiskerWidgetState
     * @param parent Parent QObject (typically nullptr, managed by WorkspaceManager)
     */
    explicit WhiskerWidgetState(QObject * parent = nullptr);

    ~WhiskerWidgetState() override = default;

    // === Type Identification ===

    /**
     * @brief Get the type name for this state
     * @return "WhiskerWidget"
     */
    [[nodiscard]] QString getTypeName() const override { return QStringLiteral("WhiskerWidget"); }

    /**
     * @brief Get the display name for UI
     * @return User-visible name (default: "Whisker Tracking")
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

    [[nodiscard]] FaceOrientation faceOrientation() const;
    [[nodiscard]] int numWhiskersToTrack() const;
    [[nodiscard]] double lengthThreshold() const;
    [[nodiscard]] int clipLength() const;
    [[nodiscard]] float linkingTolerance() const;
    [[nodiscard]] std::string const & whiskerPadKey() const;
    [[nodiscard]] float whiskerPadX() const;
    [[nodiscard]] float whiskerPadY() const;
    [[nodiscard]] bool useMaskMode() const;
    [[nodiscard]] std::string const & selectedMaskKey() const;
    [[nodiscard]] int currentWhisker() const;
    [[nodiscard]] bool autoDL() const;

    // === State Properties - Setters ===

    void setFaceOrientation(FaceOrientation orientation);
    void setNumWhiskersToTrack(int num);
    void setLengthThreshold(double threshold);
    void setClipLength(int length);
    void setLinkingTolerance(float tolerance);
    void setWhiskerPadKey(std::string const & key);
    void setWhiskerPadPosition(float x, float y);
    void setUseMaskMode(bool use_mask);
    void setSelectedMaskKey(std::string const & key);
    void setCurrentWhisker(int whisker);
    void setAutoDL(bool auto_dl);

    // === Transient Runtime State ===
    // (NOT serialized - just runtime)
    TimePosition current_position;

signals:
    /**
     * @brief Emitted when face orientation changes
     */
    void faceOrientationChanged(FaceOrientation orientation);

    /**
     * @brief Emitted when number of whiskers to track changes
     */
    void numWhiskersToTrackChanged(int num);

    /**
     * @brief Emitted when length threshold changes
     */
    void lengthThresholdChanged(double threshold);

    /**
     * @brief Emitted when clip length changes
     */
    void clipLengthChanged(int length);

    /**
     * @brief Emitted when linking tolerance changes
     */
    void linkingToleranceChanged(float tolerance);

    /**
     * @brief Emitted when whisker pad key changes
     */
    void whiskerPadKeyChanged(std::string const & key);

    /**
     * @brief Emitted when whisker pad position changes
     */
    void whiskerPadPositionChanged(float x, float y);

    /**
     * @brief Emitted when mask mode changes
     */
    void useMaskModeChanged(bool use_mask);

    /**
     * @brief Emitted when selected mask key changes
     */
    void selectedMaskKeyChanged(std::string const & key);

    /**
     * @brief Emitted when current whisker selection changes
     */
    void currentWhiskerChanged(int whisker);

    /**
     * @brief Emitted when auto DL mode changes
     */
    void autoDLChanged(bool auto_dl);

private:
    WhiskerWidgetStateData _data;
};

#endif // WHISKER_WIDGET_STATE_HPP
