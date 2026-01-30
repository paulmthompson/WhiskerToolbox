#ifndef DATA_INSPECTOR_STATE_HPP
#define DATA_INSPECTOR_STATE_HPP

/**
 * @file DataInspectorState.hpp
 * @brief State class for DataInspector_Widget
 * 
 * DataInspectorState manages the serializable state for data inspection widgets.
 * Each inspector can inspect a single data item and can be "pinned" to ignore
 * SelectionContext updates.
 * 
 * ## Key Features
 * - Tracks which data key is being inspected
 * - Pin/unpin to control SelectionContext responsiveness
 * - UI state (collapsed sections) for workspace restore
 * - Single state class for all data types (type-specific settings in JSON)
 * 
 * ## Usage
 * 
 * ```cpp
 * auto state = std::make_shared<DataInspectorState>();
 * 
 * // Set inspected data
 * state->setInspectedDataKey("my_point_data");
 * 
 * // Pin to prevent SelectionContext updates
 * state->setPinned(true);
 * 
 * // Connect to changes
 * connect(state.get(), &DataInspectorState::inspectedDataKeyChanged,
 *         this, &MyWidget::onDataKeyChanged);
 * ```
 * 
 * @see EditorState for base class documentation
 * @see SelectionContext for inter-widget communication
 */

#include "EditorState/EditorState.hpp"
#include "EditorState/StrongTypes.hpp"  // Must be before any TimePosition usage in signals
#include "TimeFrame/TimeFrame.hpp"

#include <rfl.hpp>
#include <rfl/json.hpp>

#include <nlohmann/json.hpp>

#include <string>
#include <vector>

/**
 * @brief Serializable data structure for DataInspectorState
 * 
 * This struct is designed for reflect-cpp serialization.
 * All members should be default-constructible and serializable.
 */
struct DataInspectorStateData {
    std::string inspected_data_key;  ///< Currently inspected data key
    bool is_pinned = false;          ///< Whether to ignore SelectionContext updates
    std::string display_name = "Data Inspector";  ///< User-visible name
    std::string instance_id;         ///< Unique instance ID (preserved across serialization)
    std::vector<std::string> collapsed_sections;  ///< Section IDs that are collapsed in UI
    
    // Type-specific UI state stored as JSON string for flexibility
    // This allows type-specific widgets to store their preferences without
    // needing per-type state classes
    std::string ui_state_json = "{}";
};

/**
 * @brief State class for DataInspector_Widget
 * 
 * DataInspectorState is an EditorState subclass that tracks the state of
 * a data inspector widget. Each inspector inspects one data item.
 * 
 * ## Pinning Behavior
 * 
 * When unpinned (default), the inspector responds to SelectionContext changes
 * and updates to show the newly selected data.
 * 
 * When pinned, the inspector ignores SelectionContext and continues to show
 * its current data, allowing the user to compare data across selections.
 * 
 * ## Multiple Inspectors
 * 
 * Multiple DataInspector instances can exist simultaneously. Each has its
 * own state and can be pinned independently.
 */
class DataInspectorState : public EditorState {
    Q_OBJECT

public:
    /**
     * @brief Construct a new DataInspectorState
     * @param parent Parent QObject (typically nullptr, managed by EditorRegistry)
     */
    explicit DataInspectorState(QObject * parent = nullptr);

    ~DataInspectorState() override = default;

    // === Type Identification ===

    /**
     * @brief Get the type name for this state
     * @return "DataInspector"
     */
    [[nodiscard]] QString getTypeName() const override { return QStringLiteral("DataInspector"); }

    /**
     * @brief Get the display name for UI
     * @return User-visible name (default: "Data Inspector")
     */
    [[nodiscard]] QString getDisplayName() const override;

    /**
     * @brief Set the display name
     * @param name New display name
     */
    void setDisplayName(QString const & name) override;

    // === Transient Runtime State ===
    // (NOT serialized - just runtime)
    TimePosition current_position;

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

    // === Inspector-Specific Properties ===

    /**
     * @brief Set the data key to inspect
     * @param key Data key in DataManager (empty string to clear)
     */
    void setInspectedDataKey(QString const & key);

    /**
     * @brief Get the currently inspected data key
     * @return Data key, or empty string if none
     */
    [[nodiscard]] QString inspectedDataKey() const;

    /**
     * @brief Set pinned state
     * 
     * When pinned, the inspector ignores SelectionContext updates.
     * 
     * @param pinned true to pin, false to respond to selection changes
     */
    void setPinned(bool pinned);

    /**
     * @brief Get pinned state
     * @return true if pinned
     */
    [[nodiscard]] bool isPinned() const;

    // === UI State ===

    /**
     * @brief Set whether a UI section is collapsed
     * @param section_id Identifier for the section (e.g., "properties", "export")
     * @param collapsed true if section should be collapsed
     */
    void setSectionCollapsed(QString const & section_id, bool collapsed);

    /**
     * @brief Check if a UI section is collapsed
     * @param section_id Identifier for the section
     * @return true if collapsed
     */
    [[nodiscard]] bool isSectionCollapsed(QString const & section_id) const;

    /**
     * @brief Store type-specific UI state as JSON
     * @param json JSON object with type-specific settings
     */
    void setTypeSpecificState(nlohmann::json const & json);

    /**
     * @brief Get type-specific UI state
     * @return JSON object with type-specific settings
     */
    [[nodiscard]] nlohmann::json getTypeSpecificState() const;

signals:
    /**
     * @brief Emitted when the inspected data key changes
     * @param key The new data key
     */
    void inspectedDataKeyChanged(QString const & key);

    /**
     * @brief Emitted when pinned state changes
     * @param pinned The new pinned state
     */
    void pinnedChanged(bool pinned);

    /**
     * @brief Emitted when a section's collapsed state changes
     * @param section_id The section that changed
     * @param collapsed The new collapsed state
     */
    void sectionCollapsedChanged(QString const & section_id, bool collapsed);

private:
    DataInspectorStateData _data;
};

#endif // DATA_INSPECTOR_STATE_HPP
