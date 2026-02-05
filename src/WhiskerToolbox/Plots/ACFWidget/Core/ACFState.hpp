#ifndef ACF_STATE_HPP
#define ACF_STATE_HPP

/**
 * @file ACFState.hpp
 * @brief State class for ACFWidget
 * 
 * ACFState manages the serializable state for the ACFWidget,
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
 * @brief Serializable state data for ACFWidget
 */
struct ACFStateData {
    std::string instance_id;
    std::string display_name = "Autocorrelation Function";
    std::string event_key;  ///< Key of the DigitalEventSeries to compute ACF for
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

private:
    ACFStateData _data;
};

#endif// ACF_STATE_HPP
