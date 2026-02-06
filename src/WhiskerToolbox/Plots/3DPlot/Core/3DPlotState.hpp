#ifndef THREE_D_PLOT_STATE_HPP
#define THREE_D_PLOT_STATE_HPP

/**
 * @file 3DPlotState.hpp
 * @brief State class for 3DPlotWidget
 * 
 * 3DPlotState manages the serializable state for the 3DPlotWidget,
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
 * @brief Serializable state data for 3DPlotWidget
 */
struct ThreeDPlotStateData {
    std::string instance_id;
    std::string display_name = "3D Plot";
    // Add 3D plot specific state data here as needed
};

/**
 * @brief State class for 3DPlotWidget
 * 
 * 3DPlotState is the Qt wrapper around ThreeDPlotStateData that provides
 * typed accessors and Qt signals for all state properties.
 */
class ThreeDPlotState : public EditorState {
    Q_OBJECT

public:
    /**
     * @brief Construct a new ThreeDPlotState
     * @param parent Parent QObject (typically nullptr, managed by WorkspaceManager)
     */
    explicit ThreeDPlotState(QObject * parent = nullptr);

    ~ThreeDPlotState() override = default;

    // === Type Identification ===

    /**
     * @brief Get the type name for this state
     * @return "3DPlot"
     */
    [[nodiscard]] QString getTypeName() const override { return QStringLiteral("3DPlot"); }

    /**
     * @brief Get the display name for UI
     * @return User-visible name (default: "3D Plot")
     */
    [[nodiscard]] QString getDisplayName() const override;

    /**
     * @brief Set the display name
     * @param name New display name
     */
    void setDisplayName(QString const & name) override;

    // === Direct Data Access ===

    /**
     * @brief Get const reference to underlying data
     * @return Const reference to ThreeDPlotStateData
     */
    [[nodiscard]] ThreeDPlotStateData const & data() const { return _data; }

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
     * @brief Emitted when state changes
     */
    void stateChanged();

private:
    ThreeDPlotStateData _data;
};

#endif// THREE_D_PLOT_STATE_HPP
