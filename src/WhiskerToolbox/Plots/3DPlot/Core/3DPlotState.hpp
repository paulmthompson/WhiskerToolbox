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

#include <map>
#include <memory>
#include <optional>
#include <string>
#include <vector>

/**
 * @brief Options for plotting a PointData key in the 3D plot
 */
struct ThreeDPlotDataOptions {
    std::string data_key;  ///< Key of the PointData to plot
};

/**
 * @brief Serializable state data for 3DPlotWidget
 */
struct ThreeDPlotStateData {
    std::string instance_id;
    std::string display_name = "3D Plot";
    std::string active_point_data_key = "";  // Currently selected PointData key (deprecated, kept for compatibility)
    std::map<std::string, ThreeDPlotDataOptions> plot_data_keys;  ///< Map of data key names to their options
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

    // === Active Key Accessors ===

    /**
     * @brief Get the currently active PointData key
     * @return The active key as QString
     */
    [[nodiscard]] QString getActivePointDataKey() const;

    /**
     * @brief Set the currently active PointData key
     * @param key The key to set
     */
    void setActivePointDataKey(QString const & key);

    // === Plot Data Keys Management ===

    /**
     * @brief Add a data key to the plot
     * @param data_key The PointData key to add
     */
    void addPlotDataKey(QString const & data_key);

    /**
     * @brief Remove a data key from the plot
     * @param data_key The PointData key to remove
     */
    void removePlotDataKey(QString const & data_key);

    /**
     * @brief Get all plot data keys
     * @return List of data keys currently in the plot
     */
    [[nodiscard]] std::vector<QString> getPlotDataKeys() const;

    /**
     * @brief Get options for a specific plot data key
     * @param data_key The data key
     * @return Options struct, or std::nullopt if key not found
     */
    [[nodiscard]] std::optional<ThreeDPlotDataOptions> getPlotDataKeyOptions(QString const & data_key) const;

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
     * @brief Emitted when the active PointData key changes
     * @param key The new active key
     */
    void activePointDataKeyChanged(QString const & key);

    /**
     * @brief Emitted when a plot data key is added
     * @param data_key The added data key
     */
    void plotDataKeyAdded(QString const & data_key);

    /**
     * @brief Emitted when a plot data key is removed
     * @param data_key The removed data key
     */
    void plotDataKeyRemoved(QString const & data_key);

    /**
     * @brief Emitted when state changes
     */
    void stateChanged();

private:
    ThreeDPlotStateData _data;
};

#endif// THREE_D_PLOT_STATE_HPP
