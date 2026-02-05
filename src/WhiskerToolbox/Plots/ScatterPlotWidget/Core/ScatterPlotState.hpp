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
    double x_min = 0.0;   ///< X-axis minimum value (default: 0.0)
    double x_max = 100.0; ///< X-axis maximum value (default: 100.0)
    double y_min = 0.0;   ///< Y-axis minimum value (default: 0.0)
    double y_max = 100.0; ///< Y-axis maximum value (default: 100.0)
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

    // === X-Axis Range ===

    /**
     * @brief Get the X-axis minimum value
     * @return Minimum X value
     */
    [[nodiscard]] double getXMin() const;

    /**
     * @brief Set the X-axis minimum value
     * @param x_min Minimum X value
     */
    void setXMin(double x_min);

    /**
     * @brief Get the X-axis maximum value
     * @return Maximum X value
     */
    [[nodiscard]] double getXMax() const;

    /**
     * @brief Set the X-axis maximum value
     * @param x_max Maximum X value
     */
    void setXMax(double x_max);

    // === Y-Axis Range ===

    /**
     * @brief Get the Y-axis minimum value
     * @return Minimum Y value
     */
    [[nodiscard]] double getYMin() const;

    /**
     * @brief Set the Y-axis minimum value
     * @param y_min Minimum Y value
     */
    void setYMin(double y_min);

    /**
     * @brief Get the Y-axis maximum value
     * @return Maximum Y value
     */
    [[nodiscard]] double getYMax() const;

    /**
     * @brief Set the Y-axis maximum value
     * @param y_max Maximum Y value
     */
    void setYMax(double y_max);

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
     * @brief Emitted when X-axis minimum changes
     * @param x_min New X-axis minimum value
     */
    void xMinChanged(double x_min);

    /**
     * @brief Emitted when X-axis maximum changes
     * @param x_max New X-axis maximum value
     */
    void xMaxChanged(double x_max);

    /**
     * @brief Emitted when Y-axis minimum changes
     * @param y_min New Y-axis minimum value
     */
    void yMinChanged(double y_min);

    /**
     * @brief Emitted when Y-axis maximum changes
     * @param y_max New Y-axis maximum value
     */
    void yMaxChanged(double y_max);

private:
    ScatterPlotStateData _data;
};

#endif// SCATTER_PLOT_STATE_HPP
