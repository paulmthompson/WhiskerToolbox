#ifndef TEMPORAL_PROJECTION_VIEW_STATE_HPP
#define TEMPORAL_PROJECTION_VIEW_STATE_HPP

/**
 * @file TemporalProjectionViewState.hpp
 * @brief State class for TemporalProjectionViewWidget
 * 
 * TemporalProjectionViewState manages the serializable state for the TemporalProjectionViewWidget,
 * enabling workspace save/restore and inter-widget communication via SelectionContext.
 * 
 * @see EditorState for base class documentation
 */

#include "EditorState/EditorState.hpp"

#include <rfl.hpp>
#include <rfl/json.hpp>

#include <string>

/**
 * @brief Serializable state data for TemporalProjectionViewWidget
 */
struct TemporalProjectionViewStateData {
    std::string instance_id;
    std::string display_name = "Temporal Projection View";
};

/**
 * @brief State class for TemporalProjectionViewWidget
 * 
 * TemporalProjectionViewState is the Qt wrapper around TemporalProjectionViewStateData that provides
 * typed accessors and Qt signals for all state properties.
 */
class TemporalProjectionViewState : public EditorState {
    Q_OBJECT

public:
    /**
     * @brief Construct a new TemporalProjectionViewState
     * @param parent Parent QObject (typically nullptr, managed by WorkspaceManager)
     */
    explicit TemporalProjectionViewState(QObject * parent = nullptr);

    ~TemporalProjectionViewState() override = default;

    // === Type Identification ===

    /**
     * @brief Get the type name for this state
     * @return "TemporalProjectionView"
     */
    [[nodiscard]] QString getTypeName() const override { return QStringLiteral("TemporalProjectionView"); }

    /**
     * @brief Get the display name for UI
     * @return User-visible name (default: "Temporal Projection View")
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

private:
    TemporalProjectionViewStateData _data;
};

#endif// TEMPORAL_PROJECTION_VIEW_STATE_HPP
