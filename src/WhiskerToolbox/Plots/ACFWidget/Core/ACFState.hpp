#ifndef ACF_STATE_HPP
#define ACF_STATE_HPP

/**
 * @file ACFState.hpp
 * @brief State class for ACFWidget
 *
 * ACFState manages the serializable state for the ACFWidget, with a single
 * source of truth for view state (zoom/pan) and axis ranges. HorizontalAxisState
 * and VerticalAxisState hold full axis ranges; view state holds zoom/pan.
 *
 * @see EditorState for base class documentation
 */

#include "EditorState/EditorState.hpp"
#include "Plots/Common/HorizontalAxisWidget/Core/HorizontalAxisStateData.hpp"
#include "Plots/Common/HorizontalAxisWidget/Core/HorizontalAxisState.hpp"
#include "Plots/Common/VerticalAxisWidget/Core/VerticalAxisStateData.hpp"
#include "Plots/Common/VerticalAxisWidget/Core/VerticalAxisState.hpp"

#include <rfl.hpp>
#include <rfl/json.hpp>

#include <memory>
#include <optional>
#include <string>

/**
 * @brief View state for the ACF plot (zoom and pan only)
 *
 * Data bounds come from HorizontalAxisState and VerticalAxisState.
 * This struct only holds the view transform.
 */
struct ACFViewState {
    double x_zoom = 1.0;
    double y_zoom = 1.0;
    double x_pan = 0.0;
    double y_pan = 0.0;
};

/**
 * @brief Serializable state data for ACFWidget
 */
struct ACFStateData {
    std::string instance_id;
    std::string display_name = "Autocorrelation Function";
    std::string event_key;  ///< Key of the DigitalEventSeries to compute ACF for
    ACFViewState view_state;
    HorizontalAxisStateData horizontal_axis;
    VerticalAxisStateData vertical_axis;
};

/**
 * @brief State class for ACFWidget
 * 
 * ACFState is the Qt wrapper around ACFStateData that provides
 * typed accessors and Qt signals for all state properties.
 */
class ACFState : public EditorState {
    Q_OBJECT

public:
    /**
     * @brief Construct a new ACFState
     * @param parent Parent QObject (typically nullptr, managed by WorkspaceManager)
     */
    explicit ACFState(QObject * parent = nullptr);

    ~ACFState() override = default;

    // === Type Identification ===

    /**
     * @brief Get the type name for this state
     * @return "ACF"
     */
    [[nodiscard]] QString getTypeName() const override { return QStringLiteral("ACF"); }

    /**
     * @brief Get the display name for UI
     * @return User-visible name (default: "Autocorrelation Function")
     */
    [[nodiscard]] QString getDisplayName() const override;

    /**
     * @brief Set the display name
     * @param name New display name
     */
    void setDisplayName(QString const & name) override;

    // === Event Key ===

    /**
     * @brief Get the event data key
     * @return Key of the selected DigitalEventSeries
     */
    [[nodiscard]] QString getEventKey() const;

    /**
     * @brief Set the event data key
     * @param key Key of the DigitalEventSeries to use for ACF computation
     */
    void setEventKey(QString const & key);

    // === Axis state access (for widgets and serialization) ===
    [[nodiscard]] HorizontalAxisState * horizontalAxisState() { return _horizontal_axis_state.get(); }
    [[nodiscard]] VerticalAxisState * verticalAxisState() { return _vertical_axis_state.get(); }

    // === View state (zoom / pan) ===
    [[nodiscard]] ACFViewState const & viewState() const { return _data.view_state; }
    void setXZoom(double zoom);
    void setYZoom(double zoom);
    void setPan(double x_pan, double y_pan);

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
    /**
     * @brief Emitted when event key changes
     * @param key New event key
     */
    void eventKeyChanged(QString const & key);

    void viewStateChanged();

private:
    ACFStateData _data;
    std::unique_ptr<HorizontalAxisState> _horizontal_axis_state;
    std::unique_ptr<VerticalAxisState> _vertical_axis_state;
};

#endif// ACF_STATE_HPP
