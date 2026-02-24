#ifndef GROUP_MANAGEMENT_WIDGET_STATE_HPP
#define GROUP_MANAGEMENT_WIDGET_STATE_HPP

/**
 * @file GroupManagementWidgetState.hpp
 * @brief State class for GroupManagementWidget
 * 
 * GroupManagementWidgetState manages the serializable state for the GroupManagementWidget,
 * enabling workspace save/restore and inter-widget communication via SelectionContext.
 * 
 * This tracks:
 * - Selected group ID in the group table
 * - Expanded/collapsed state of group sections
 * 
 * @see EditorState for base class documentation
 * @see SelectionContext for inter-widget communication
 */

#include "GroupManagementWidget/GroupManagementWidgetStateData.hpp"
#include "EditorState/EditorState.hpp"

#include <rfl.hpp>
#include <rfl/json.hpp>

#include <string>
#include <vector>

/**
 * @brief State class for GroupManagementWidget
 * 
 * GroupManagementWidgetState is a minimal EditorState subclass that tracks
 * the selected group in the GroupManagementWidget's table.
 * 
 * ## Usage
 * 
 * ```cpp
 * // Create state (typically done by EditorRegistry)
 * auto state = std::make_shared<GroupManagementWidgetState>();
 * 
 * // Connect to selection changes
 * connect(state.get(), &GroupManagementWidgetState::selectedGroupChanged,
 *         this, &MyWidget::onGroupChanged);
 * 
 * // Update selection
 * state->setSelectedGroupId(5);
 * 
 * // Serialize for workspace save
 * std::string json = state->toJson();
 * ```
 */
class GroupManagementWidgetState : public EditorState {
    Q_OBJECT

public:
    /**
     * @brief Construct a new GroupManagementWidgetState
     * @param parent Parent QObject (typically nullptr, managed by EditorRegistry)
     */
    explicit GroupManagementWidgetState(QObject * parent = nullptr);

    ~GroupManagementWidgetState() override = default;

    // === Type Identification ===

    /**
     * @brief Get the type name for this state
     * @return "GroupManagementWidget"
     */
    [[nodiscard]] QString getTypeName() const override { return QStringLiteral("GroupManagementWidget"); }

    /**
     * @brief Get the display name for UI
     * @return User-visible name (default: "Group Manager")
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
     * @brief Set the selected group ID
     * 
     * This represents the currently highlighted/selected group in the
     * GroupManagementWidget's table.
     * 
     * @param group_id Group ID to select (-1 to clear selection)
     */
    void setSelectedGroupId(int group_id);

    /**
     * @brief Get the selected group ID
     * @return Currently selected group ID, or -1 if none
     */
    [[nodiscard]] int selectedGroupId() const;

signals:
    /**
     * @brief Emitted when the selected group changes
     * @param group_id The new selected group ID (-1 if none)
     */
    void selectedGroupChanged(int group_id);

private:
    GroupManagementWidgetStateData _data;
};

#endif // GROUP_MANAGEMENT_WIDGET_STATE_HPP
