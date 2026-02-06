#ifndef SPECTROGRAM_STATE_HPP
#define SPECTROGRAM_STATE_HPP

/**
 * @file SpectrogramState.hpp
 * @brief State class for SpectrogramWidget
 * 
 * SpectrogramState manages the serializable state for the SpectrogramWidget,
 * enabling workspace save/restore and inter-widget communication via SelectionContext.
 * 
 * @see EditorState for base class documentation
 */

#include "EditorState/EditorState.hpp"

#include <rfl.hpp>
#include <rfl/json.hpp>

#include <memory>
#include <optional>
#include <string>

/**
 * @brief View state for the spectrogram (zoom, pan, bounds)
 */
struct SpectrogramViewState {
    // === Data Bounds (changing these triggers scene rebuild) ===
    
    /// Time before alignment in ms (typically negative). Defines data window start.
    double x_min = -500.0;
    
    /// Time after alignment in ms (typically positive). Defines data window end.
    double x_max = 500.0;
    
    // === View Transform (changing these only updates projection matrix) ===
    
    /// X-axis (time) zoom factor. 1.0 = full window, 2.0 = show half the window.
    double x_zoom = 1.0;
    
    /// Y-axis (frequency) zoom factor. 1.0 = all frequencies fit, 2.0 = half the frequencies fit.
    double y_zoom = 1.0;
    
    /// X-axis pan offset in world units (ms). Positive = view shifts right.
    double x_pan = 0.0;
    
    /// Y-axis pan offset in normalized units. Positive = view shifts up.
    double y_pan = 0.0;
};

/**
 * @brief Axis labeling and grid options
 */
struct SpectrogramAxisOptions {
    std::string x_label = "Time (ms)";  ///< X-axis label
    std::string y_label = "Frequency (Hz)";  ///< Y-axis label
    bool show_x_axis = true;             ///< Whether to show X axis
    bool show_y_axis = true;             ///< Whether to show Y axis
    bool show_grid = false;              ///< Whether to show grid lines
};

/**
 * @brief Serializable state data for SpectrogramWidget
 */
struct SpectrogramStateData {
    std::string instance_id;
    std::string display_name = "Spectrogram";
    SpectrogramViewState view_state;                               ///< View state (zoom, pan, bounds)
    SpectrogramAxisOptions axis_options;                           ///< Axis labels and grid options
    std::string background_color = "#FFFFFF";                     ///< Background color as hex string (default: white)
    bool pinned = false;                                           ///< Whether to ignore SelectionContext changes
    std::string analog_signal_key;                                ///< Key of the analog signal to display
};

/**
 * @brief State class for SpectrogramWidget
 * 
 * SpectrogramState is the Qt wrapper around SpectrogramStateData that provides
 * typed accessors and Qt signals for all state properties.
 */
class SpectrogramState : public EditorState {
    Q_OBJECT

public:
    /**
     * @brief Construct a new SpectrogramState
     * @param parent Parent QObject (typically nullptr, managed by WorkspaceManager)
     */
    explicit SpectrogramState(QObject * parent = nullptr);

    ~SpectrogramState() override = default;

    // === Type Identification ===

    /**
     * @brief Get the type name for this state
     * @return "Spectrogram"
     */
    [[nodiscard]] QString getTypeName() const override { return QStringLiteral("Spectrogram"); }

    /**
     * @brief Get the display name for UI
     * @return User-visible name (default: "Spectrogram")
     */
    [[nodiscard]] QString getDisplayName() const override;

    /**
     * @brief Set the display name
     * @param name New display name
     */
    void setDisplayName(QString const & name) override;

    // === Window Size ===

    /**
     * @brief Get the window size
     * @return Window size in time units
     */
    [[nodiscard]] double getWindowSize() const;


    // === View State ===

    /**
     * @brief Get the view state (zoom, pan, bounds)
     * @return Const reference to the view state
     */
    [[nodiscard]] SpectrogramViewState const & viewState() const { return _data.view_state; }

    /**
     * @brief Set the complete view state
     * @param view_state New view state
     */
    void setViewState(SpectrogramViewState const & view_state);

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
     * Adjusts frequency density without changing the underlying data.
     * Only emits viewStateChanged(), not stateChanged() - no scene rebuild.
     * 
     * @param zoom Zoom factor (1.0 = all frequencies fit, 2.0 = half the frequencies fit)
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
    [[nodiscard]] SpectrogramAxisOptions const & axisOptions() const { return _data.axis_options; }

    /**
     * @brief Set axis options
     * @param options New axis options
     */
    void setAxisOptions(SpectrogramAxisOptions const & options);

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

    // === Analog Signal Key ===

    /**
     * @brief Get the analog signal key
     * @return Key of the analog signal to display
     */
    [[nodiscard]] QString getAnalogSignalKey() const;

    /**
     * @brief Set the analog signal key
     * @param key Key of the analog signal to display
     */
    void setAnalogSignalKey(QString const & key);

    // === Direct Data Access ===

    /**
     * @brief Get const reference to underlying data
     * @return Const reference to SpectrogramStateData
     */
    [[nodiscard]] SpectrogramStateData const & data() const { return _data; }

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
     * @brief Emitted when analog signal key changes
     * @param key New analog signal key
     */
    void analogSignalKeyChanged(QString const & key);

private:
    SpectrogramStateData _data;
};

#endif// SPECTROGRAM_STATE_HPP
