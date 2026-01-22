#ifndef EXPORT_VIDEO_WIDGET_STATE_HPP
#define EXPORT_VIDEO_WIDGET_STATE_HPP

/**
 * @file ExportVideoWidgetState.hpp
 * @brief State class for Export_Video_Widget
 * 
 * ExportVideoWidgetState manages the serializable state for the Export_Video_Widget,
 * enabling workspace save/restore and integration with the EditorRegistry system.
 * 
 * State tracked:
 * - Selected media widget for export
 * - Output filename and settings
 * - Frame range settings
 * 
 * @see EditorState for base class documentation
 * @see Export_Video_Widget for the widget implementation
 */

#include "EditorState/EditorState.hpp"

#include <rfl.hpp>
#include <rfl/json.hpp>

#include <string>

/**
 * @brief Serializable data structure for ExportVideoWidgetState
 * 
 * This struct is designed for reflect-cpp serialization.
 * All members should be default-constructible and serializable.
 */
struct ExportVideoWidgetStateData {
    std::string selected_media_widget_id;  ///< ID of the selected media widget for export
    std::string output_filename;           ///< Output filename
    int start_frame = 0;                   ///< Start frame for export
    int end_frame = 0;                     ///< End frame for export
    int frame_rate = 30;                   ///< Output frame rate
    int output_width = 640;                ///< Output video width
    int output_height = 480;               ///< Output video height
    std::string instance_id;               ///< Unique instance ID (preserved across serialization)
    std::string display_name = "Video Export";  ///< User-visible name
};

/**
 * @brief State class for Export_Video_Widget
 * 
 * ExportVideoWidgetState is a single-instance widget state that manages
 * the video export configuration.
 * 
 * ## Usage
 * 
 * ```cpp
 * // Create state (typically done by EditorCreationController)
 * auto state = std::make_shared<ExportVideoWidgetState>();
 * registry->registerState(state);
 * 
 * // Serialize for workspace save
 * std::string json = state->toJson();
 * ```
 */
class ExportVideoWidgetState : public EditorState {
    Q_OBJECT

public:
    /**
     * @brief Construct a new ExportVideoWidgetState
     * @param parent Parent QObject (typically nullptr, managed by WorkspaceManager)
     */
    explicit ExportVideoWidgetState(QObject * parent = nullptr);

    ~ExportVideoWidgetState() override = default;

    // === Type Identification ===

    /**
     * @brief Get the type name for this state
     * @return "ExportVideoWidget"
     */
    [[nodiscard]] QString getTypeName() const override { return QStringLiteral("ExportVideoWidget"); }

    /**
     * @brief Get the display name for UI
     * @return User-visible name (default: "Video Export")
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
     * @brief Set the selected media widget ID for export
     * @param id Media widget instance ID
     */
    void setSelectedMediaWidgetId(QString const & id);

    /**
     * @brief Get the selected media widget ID
     * @return Currently selected media widget ID, or empty string if none
     */
    [[nodiscard]] QString selectedMediaWidgetId() const;

    /**
     * @brief Set the output filename
     * @param filename Output filename
     */
    void setOutputFilename(QString const & filename);

    /**
     * @brief Get the output filename
     * @return Output filename
     */
    [[nodiscard]] QString outputFilename() const;

    /**
     * @brief Set the start frame
     * @param frame Start frame number
     */
    void setStartFrame(int frame);

    /**
     * @brief Get the start frame
     * @return Start frame number
     */
    [[nodiscard]] int startFrame() const;

    /**
     * @brief Set the end frame
     * @param frame End frame number
     */
    void setEndFrame(int frame);

    /**
     * @brief Get the end frame
     * @return End frame number
     */
    [[nodiscard]] int endFrame() const;

    /**
     * @brief Set the frame rate
     * @param rate Frame rate (fps)
     */
    void setFrameRate(int rate);

    /**
     * @brief Get the frame rate
     * @return Frame rate (fps)
     */
    [[nodiscard]] int frameRate() const;

    /**
     * @brief Set the output width
     * @param width Output video width
     */
    void setOutputWidth(int width);

    /**
     * @brief Get the output width
     * @return Output video width
     */
    [[nodiscard]] int outputWidth() const;

    /**
     * @brief Set the output height
     * @param height Output video height
     */
    void setOutputHeight(int height);

    /**
     * @brief Get the output height
     * @return Output video height
     */
    [[nodiscard]] int outputHeight() const;

signals:
    /**
     * @brief Emitted when the selected media widget changes
     * @param id The new media widget ID
     */
    void selectedMediaWidgetIdChanged(QString const & id);

    /**
     * @brief Emitted when the output filename changes
     * @param filename The new filename
     */
    void outputFilenameChanged(QString const & filename);

    /**
     * @brief Emitted when the start frame changes
     * @param frame The new start frame
     */
    void startFrameChanged(int frame);

    /**
     * @brief Emitted when the end frame changes
     * @param frame The new end frame
     */
    void endFrameChanged(int frame);

    /**
     * @brief Emitted when the frame rate changes
     * @param rate The new frame rate
     */
    void frameRateChanged(int rate);

    /**
     * @brief Emitted when the output dimensions change
     * @param width The new width
     * @param height The new height
     */
    void outputDimensionsChanged(int width, int height);

private:
    ExportVideoWidgetStateData _data;
};

#endif // EXPORT_VIDEO_WIDGET_STATE_HPP
