#ifndef DATA_MANAGER_WIDGET_STATE_HPP
#define DATA_MANAGER_WIDGET_STATE_HPP

/**
 * @file DataManagerWidgetState.hpp
 * @brief State class for DataManager_Widget
 * 
 * DataManagerWidgetState manages the serializable state for the DataManager_Widget,
 * enabling workspace save/restore and inter-widget communication via SelectionContext.
 * 
 * This is a minimal implementation for Phase 2.1 that tracks:
 * - Selected data key in the feature table
 * 
 * Future phases will add:
 * - Expanded/collapsed state of data sections
 * - Filter settings
 * - View preferences
 * 
 * @see EditorState for base class documentation
 * @see SelectionContext for inter-widget communication
 */

#include "EditorState/EditorState.hpp"

#include <rfl.hpp>
#include <rfl/json.hpp>

#include <string>

/**
 * @brief Serializable data structure for DataManagerWidgetState
 * 
 * This struct is designed for reflect-cpp serialization.
 * All members should be default-constructible and serializable.
 */
struct DataManagerWidgetStateData {
    std::string selected_data_key;  ///< Currently selected data key in feature table
    std::string instance_id;        ///< Unique instance ID (preserved across serialization)
    std::string display_name = "Data Manager";  ///< User-visible name
};

/**
 * @brief State class for DataManager_Widget
 * 
 * DataManagerWidgetState is a minimal EditorState subclass that tracks
 * the selected data key in the DataManager_Widget's feature table.
 * 
 * ## Usage
 * 
 * ```cpp
 * // Create state (typically done by WorkspaceManager)
 * auto state = std::make_shared<DataManagerWidgetState>();
 * 
 * // Connect to selection changes
 * connect(state.get(), &DataManagerWidgetState::selectedDataKeyChanged,
 *         this, &MyWidget::onDataKeyChanged);
 * 
 * // Update selection
 * state->setSelectedDataKey("my_data_key");
 * 
 * // Serialize for workspace save
 * std::string json = state->toJson();
 * ```
 * 
 * ## Integration with SelectionContext
 * 
 * The widget should forward state changes to SelectionContext:
 * 
 * ```cpp
 * connect(_state.get(), &DataManagerWidgetState::selectedDataKeyChanged,
 *         this, [this](QString const& key) {
 *     SelectionSource source{_state->getInstanceId(), "feature_table"};
 *     _selection_context->setSelectedData(key, source);
 * });
 * ```
 */
class DataManagerWidgetState : public EditorState {
    Q_OBJECT

public:
    /**
     * @brief Construct a new DataManagerWidgetState
     * @param parent Parent QObject (typically nullptr, managed by WorkspaceManager)
     */
    explicit DataManagerWidgetState(QObject * parent = nullptr);

    ~DataManagerWidgetState() override = default;

    // === Type Identification ===

    /**
     * @brief Get the type name for this state
     * @return "DataManagerWidget"
     */
    [[nodiscard]] QString getTypeName() const override { return QStringLiteral("DataManagerWidget"); }

    /**
     * @brief Get the display name for UI
     * @return User-visible name (default: "Data Manager")
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
     * @brief Set the selected data key
     * 
     * This represents the currently highlighted/selected data in the
     * DataManager_Widget's feature table.
     * 
     * @param key Data key to select (empty string to clear selection)
     */
    void setSelectedDataKey(QString const & key);

    /**
     * @brief Get the selected data key
     * @return Currently selected data key, or empty string if none
     */
    [[nodiscard]] QString selectedDataKey() const;

signals:
    /**
     * @brief Emitted when the selected data key changes
     * @param key The new selected data key
     */
    void selectedDataKeyChanged(QString const & key);

private:
    DataManagerWidgetStateData _data;
};

#endif // DATA_MANAGER_WIDGET_STATE_HPP
