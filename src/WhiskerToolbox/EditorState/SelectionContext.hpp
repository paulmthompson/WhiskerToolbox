#ifndef SELECTION_CONTEXT_HPP
#define SELECTION_CONTEXT_HPP

/**
 * @file SelectionContext.hpp
 * @brief Centralized selection and focus management for inter-widget communication
 * 
 * SelectionContext provides a single source of truth for:
 * - Which data objects are currently selected
 * - Which entities within data objects are selected
 * - Which editor has focus
 * - Interaction history for properties panel routing
 * 
 * Widgets observe SelectionContext to stay synchronized with the
 * application's selection state.
 * 
 * @see EditorState for widget state management
 * @see WorkspaceManager for state registry
 */

#include <QObject>
#include <QString>

#include <cstdint>
#include <optional>
#include <set>
#include <string>
#include <vector>

/**
 * @brief Identifies the source of a selection change
 * 
 * When processing selection changes, widgets can check if the change
 * came from themselves to avoid circular updates:
 * 
 * ```cpp
 * void MyWidget::onSelectionChanged(SelectionSource const& source) {
 *     if (source.editor_instance_id == _my_state->getInstanceId()) {
 *         return; // Ignore our own selection change
 *     }
 *     // Handle selection from other widget
 * }
 * ```
 */
struct SelectionSource {
    QString editor_instance_id;  ///< Instance ID of the editor that made the selection
    QString widget_id;           ///< Specific widget within editor (optional, for compound editors)

    bool operator==(SelectionSource const & other) const {
        return editor_instance_id == other.editor_instance_id && widget_id == other.widget_id;
    }
};

/**
 * @brief Represents a selected data item with optional specificity
 * 
 * SelectedItem can represent:
 * - Just a data key (selecting entire data object)
 * - Data key + entity ID (selecting specific entity)
 * - Data key + time index (selecting specific frame)
 * - All three for maximum specificity
 */
struct SelectedItem {
    QString data_key;                    ///< Key in DataManager
    std::optional<int64_t> entity_id;    ///< Specific entity within data (optional)
    std::optional<int> time_index;       ///< Specific time frame (optional)

    bool operator<(SelectedItem const & other) const {
        if (data_key != other.data_key) return data_key < other.data_key;
        if (entity_id != other.entity_id) return entity_id < other.entity_id;
        return time_index < other.time_index;
    }

    bool operator==(SelectedItem const & other) const {
        return data_key == other.data_key && entity_id == other.entity_id &&
               time_index == other.time_index;
    }
};

/**
 * @brief Context for determining which properties panel to show
 * 
 * PropertiesContext captures the information needed to route
 * the user to appropriate properties panels based on their
 * interaction pattern.
 */
struct PropertiesContext {
    QString last_interacted_editor;  ///< Editor that had last meaningful interaction
    QString selected_data_key;       ///< Currently selected data
    QString data_type;               ///< Type of selected data (e.g., "LineData", "MaskData")
};

/**
 * @brief Centralized selection and focus context for the application
 * 
 * SelectionContext is a singleton-like object (owned by WorkspaceManager)
 * that manages application-wide selection state. All widgets that need
 * to know about or modify selection should interact with SelectionContext.
 * 
 * ## Key Concepts
 * 
 * ### Data Selection
 * One or more data objects (by key) can be selected. There is always
 * a "primary" selection which is the most recently selected item.
 * 
 * ### Entity Selection
 * Within the currently selected data, specific entities can be selected.
 * This is useful for line/mask/point data where each element has an EntityID.
 * 
 * ### Active Editor
 * The editor that currently has focus. This affects keyboard shortcuts
 * and determines which editor's commands are available.
 * 
 * ### Properties Context
 * Determines which properties panel should be shown. This follows
 * "last meaningful interaction" logic - if the user clicks on data
 * in the data manager, then clicks in a media widget (but doesn't
 * interact with it), the properties for the data manager selection
 * should persist.
 * 
 * ## Usage Pattern
 * 
 * ```cpp
 * // In a widget constructor
 * connect(_selection_context, &SelectionContext::selectionChanged,
 *         this, &MyWidget::onSelectionChanged);
 * 
 * // Making a selection
 * SelectionSource source{_state->getInstanceId(), "my_table"};
 * _selection_context->setSelectedData("data_key", source);
 * 
 * // Querying selection
 * if (_selection_context->isSelected("data_key")) {
 *     highlightData("data_key");
 * }
 * ```
 */
class SelectionContext : public QObject {
    Q_OBJECT

public:
    /**
     * @brief Construct a new SelectionContext
     * @param parent Parent QObject (typically WorkspaceManager)
     */
    explicit SelectionContext(QObject * parent = nullptr);

    ~SelectionContext() override = default;

    // === Data Selection ===

    /**
     * @brief Set the primary selected data key
     * 
     * This replaces any existing selection with a single item.
     * 
     * @param data_key Key in DataManager
     * @param source Who is making this selection
     */
    void setSelectedData(QString const & data_key, SelectionSource const & source);

    /**
     * @brief Add to the current selection (for multi-select)
     * 
     * The first selected item remains the primary selection.
     * 
     * @param data_key Key to add to selection
     * @param source Who is making this selection
     */
    void addToSelection(QString const & data_key, SelectionSource const & source);

    /**
     * @brief Remove from current selection
     * 
     * If the removed item was primary, the next item becomes primary.
     * 
     * @param data_key Key to remove from selection
     * @param source Who is making this change
     */
    void removeFromSelection(QString const & data_key, SelectionSource const & source);

    /**
     * @brief Clear all selections
     * @param source Who is clearing the selection
     */
    void clearSelection(SelectionSource const & source);

    /**
     * @brief Get primary selected data key
     * @return Primary selection, or empty QString if nothing selected
     */
    [[nodiscard]] QString primarySelectedData() const;

    /**
     * @brief Get all selected data keys
     * @return Set of all selected data keys
     */
    [[nodiscard]] std::set<QString> allSelectedData() const;

    /**
     * @brief Check if specific data is selected
     * @param data_key Key to check
     * @return true if data_key is in the current selection
     */
    [[nodiscard]] bool isSelected(QString const & data_key) const;

    // === Entity Selection ===

    /**
     * @brief Set selected entities within current data
     * 
     * Entity selection is secondary to data selection. When data
     * selection changes, entity selection is typically cleared.
     * 
     * @param entity_ids EntityIDs to select
     * @param source Who is making this selection
     */
    void setSelectedEntities(std::vector<int64_t> const & entity_ids, SelectionSource const & source);

    /**
     * @brief Add entities to selection
     * @param entity_ids EntityIDs to add
     * @param source Who is making this selection
     */
    void addSelectedEntities(std::vector<int64_t> const & entity_ids, SelectionSource const & source);

    /**
     * @brief Clear entity selection
     * @param source Who is clearing
     */
    void clearEntitySelection(SelectionSource const & source);

    /**
     * @brief Get selected entity IDs
     * @return Vector of selected EntityIDs
     */
    [[nodiscard]] std::vector<int64_t> selectedEntities() const;

    /**
     * @brief Check if a specific entity is selected
     * @param entity_id EntityID to check
     * @return true if entity is selected
     */
    [[nodiscard]] bool isEntitySelected(int64_t entity_id) const;

    // === Active Editor ===

    /**
     * @brief Set the currently active (focused) editor
     * 
     * Called when an editor gains focus. This affects:
     * - Which editor receives keyboard shortcuts
     * - Default target for actions
     * 
     * @param instance_id Instance ID of the active editor
     */
    void setActiveEditor(QString const & instance_id);

    /**
     * @brief Get the active editor instance ID
     * @return Instance ID, or empty QString if none active
     */
    [[nodiscard]] QString activeEditorId() const;

    // === Properties Context ===

    /**
     * @brief Get current properties context
     * 
     * Used by PropertiesHost to determine which properties panel to show.
     * 
     * @return Current PropertiesContext
     */
    [[nodiscard]] PropertiesContext propertiesContext() const;

    /**
     * @brief Notify that an editor had meaningful user interaction
     * 
     * "Meaningful" interaction includes:
     * - Clicking on content (not just focus)
     * - Modifying data
     * - Using tools
     * 
     * This updates the properties context so the appropriate
     * properties panel can be shown.
     * 
     * @param editor_instance_id Instance ID of the interacted editor
     */
    void notifyInteraction(QString const & editor_instance_id);

    /**
     * @brief Set data type for properties context
     * 
     * Called when the selected data's type is known, to help
     * properties routing.
     * 
     * @param data_type Type string (e.g., "LineData", "MaskData")
     */
    void setSelectedDataType(QString const & data_type);

signals:
    /**
     * @brief Emitted when data selection changes
     * @param source Who made the selection change
     */
    void selectionChanged(SelectionSource const & source);

    /**
     * @brief Emitted when entity selection changes
     * @param source Who made the selection change
     */
    void entitySelectionChanged(SelectionSource const & source);

    /**
     * @brief Emitted when active editor changes
     * @param instance_id Instance ID of the new active editor
     */
    void activeEditorChanged(QString const & instance_id);

    /**
     * @brief Emitted when properties context changes
     * 
     * PropertiesHost listens to this to switch property panels.
     */
    void propertiesContextChanged();

private:
    QString _primary_selected;
    std::set<QString> _selected_data;
    std::vector<int64_t> _selected_entities;
    QString _active_editor_id;
    QString _last_interacted_editor;
    QString _selected_data_type;
};

#endif // SELECTION_CONTEXT_HPP
