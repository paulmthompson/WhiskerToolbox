#ifndef SCATTER_PLOT_STATE_HPP
#define SCATTER_PLOT_STATE_HPP

/**
 * @file ScatterPlotState.hpp
 * @brief State class for ScatterPlotWidget
 * 
 * ScatterPlotState manages the serializable state for the ScatterPlotWidget,
 * enabling workspace save/restore and inter-widget communication via SelectionContext.
 * 
 * @see EditorState for base class documentation
 */

#include "EditorState/EditorState.hpp"

#include <rfl.hpp>
#include <rfl/json.hpp>

#include <string>

/**
 * @brief Serializable state data for ScatterPlotWidget
 */
struct ScatterPlotStateData {
    std::string instance_id;
    std::string display_name = "Scatter Plot";
};

/**
 * @brief State class for ScatterPlotWidget
 * 
 * ScatterPlotState is the Qt wrapper around ScatterPlotStateData that provides
 * typed accessors and Qt signals for all state properties.
 */
class ScatterPlotState : public EditorState {
    Q_OBJECT

public:
    /**
     * @brief Construct a new ScatterPlotState
     * @param parent Parent QObject (typically nullptr, managed by WorkspaceManager)
     */
    explicit ScatterPlotState(QObject * parent = nullptr);

    ~ScatterPlotState() override = default;

    // === Type Identification ===

    /**
     * @brief Get the type name for this state
     * @return "ScatterPlot"
     */
    [[nodiscard]] QString getTypeName() const override { return QStringLiteral("ScatterPlot"); }

    /**
     * @brief Get the display name for UI
     * @return User-visible name (default: "Scatter Plot")
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
    ScatterPlotStateData _data;
};

#endif// SCATTER_PLOT_STATE_HPP
