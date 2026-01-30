#ifndef GROUP_FILTER_HELPER_HPP
#define GROUP_FILTER_HELPER_HPP

/**
 * @file GroupFilterHelper.hpp
 * @brief Helper functions for group filter combo box management and move/copy operations
 * 
 * Provides shared functionality for populating and managing group filter
 * combo boxes in inspector widgets, as well as move/copy operations for
 * RaggedTimeSeries-derived data types. This allows LineInspector and MaskInspector
 * to share common logic.
 * 
 * @see LineInspector
 * @see MaskInspector
 */

#include "DataManager/DataManager.hpp"
#include "Entity/EntityTypes.hpp"
#include "GroupManagementWidget/GroupManager.hpp"
#include "Observer/Observer_Data.hpp"

#include <QComboBox>
#include <QObject>
#include <QString>

#include <iostream>
#include <string>
#include <unordered_set>
#include <vector>

/**
 * @brief Populate a group filter combo box with available groups
 * 
 * Clears the combo box and populates it with:
 * - "All Groups" as the first item (index 0)
 * - All groups from the GroupManager (if provided)
 * 
 * Signals are blocked during population to avoid triggering filter changes.
 * 
 * @param combo_box The combo box to populate
 * @param group_manager Pointer to GroupManager (can be nullptr)
 * 
 * @post combo_box contains "All Groups" plus all groups from group_manager
 * @post combo_box current index is set to 0 ("All Groups") if no valid selection exists
 */
void populateGroupFilterCombo(QComboBox * combo_box, GroupManager * group_manager);

/**
 * @brief Connect GroupManager signals to a slot for updating group filter combo
 * 
 * Connects the groupCreated, groupRemoved, and groupModified signals from
 * GroupManager to the provided slot. This allows the combo box to automatically
 * update when groups change.
 * 
 * @param group_manager Pointer to GroupManager (can be nullptr)
 * @param receiver Object that will receive the signals
 * @param slot Slot function to call when groups change
 * 
 * @note Uses Qt5-style connect syntax. The slot should repopulate the combo box.
 */
template<typename Receiver>
void connectGroupManagerSignals(GroupManager * group_manager, Receiver * receiver, void (Receiver::*slot)()) {
    if (group_manager && receiver) {
        QObject::connect(group_manager, &GroupManager::groupCreated, receiver, slot);
        QObject::connect(group_manager, &GroupManager::groupRemoved, receiver, slot);
        QObject::connect(group_manager, &GroupManager::groupModified, receiver, slot);
    }
}

/**
 * @brief Restore selection in group filter combo box after repopulation
 * 
 * Attempts to restore the previous selection after the combo box has been
 * repopulated. Tries to restore by index first, then by text if index fails.
 * Falls back to "All Groups" (index 0) if the previous selection is no longer valid.
 * 
 * @param combo_box The combo box to restore selection in
 * @param previous_index The index that was selected before repopulation
 * @param previous_text The text that was selected before repopulation (optional)
 * 
 * @post combo_box has a valid selection (at least index 0)
 */
void restoreGroupFilterSelection(QComboBox * combo_box, int previous_index, QString const & previous_text = QString());

/**
 * @brief Move selected entities from source to target data
 * 
 * Template function that moves entities by EntityId from a source RaggedTimeSeries
 * to a target RaggedTimeSeries. Works with any RaggedTimeSeries-derived type
 * (LineData, MaskData, PointData, etc.).
 * 
 * @tparam DataType The RaggedTimeSeries-derived type (e.g., LineData, MaskData)
 * @param data_manager Pointer to DataManager
 * @param active_key Source data key
 * @param target_key Target data key
 * @param selected_entity_ids Vector of EntityIds to move
 * @return Number of entities successfully moved
 * 
 * @pre data_manager != nullptr
 * @pre !active_key.empty()
 * @pre !target_key.empty()
 * @pre Source and target data exist in data_manager
 * @post Selected entities are moved from source to target
 */
template<typename DataType>
std::size_t moveEntitiesByIds(
    DataManager * data_manager,
    std::string const & active_key,
    std::string const & target_key,
    std::vector<EntityId> const & selected_entity_ids) {
    if (!data_manager || active_key.empty() || target_key.empty() || selected_entity_ids.empty()) {
        return 0;
    }

    auto source_data = data_manager->getData<DataType>(active_key);
    auto target_data = data_manager->getData<DataType>(target_key);

    if (!source_data) {
        std::cerr << "moveEntitiesByIds: Source " << typeid(DataType).name() 
                  << " object ('" << active_key << "') not found." << std::endl;
        return 0;
    }
    if (!target_data) {
        std::cerr << "moveEntitiesByIds: Target " << typeid(DataType).name() 
                  << " object ('" << target_key << "') not found." << std::endl;
        return 0;
    }

    std::unordered_set<EntityId> const selected_entity_ids_set(selected_entity_ids.begin(), selected_entity_ids.end());
    std::size_t const total_moved = source_data->moveByEntityIds(*target_data, selected_entity_ids_set, NotifyObservers::Yes);

    return total_moved;
}

/**
 * @brief Copy selected entities from source to target data
 * 
 * Template function that copies entities by EntityId from a source RaggedTimeSeries
 * to a target RaggedTimeSeries. Works with any RaggedTimeSeries-derived type
 * (LineData, MaskData, PointData, etc.).
 * 
 * @tparam DataType The RaggedTimeSeries-derived type (e.g., LineData, MaskData)
 * @param data_manager Pointer to DataManager
 * @param active_key Source data key
 * @param target_key Target data key
 * @param selected_entity_ids Vector of EntityIds to copy
 * @return Number of entities successfully copied
 * 
 * @pre data_manager != nullptr
 * @pre !active_key.empty()
 * @pre !target_key.empty()
 * @pre Source and target data exist in data_manager
 * @post Selected entities are copied from source to target (source unchanged)
 */
template<typename DataType>
std::size_t copyEntitiesByIds(
    DataManager * data_manager,
    std::string const & active_key,
    std::string const & target_key,
    std::vector<EntityId> const & selected_entity_ids) {
    if (!data_manager || active_key.empty() || target_key.empty() || selected_entity_ids.empty()) {
        return 0;
    }

    auto source_data = data_manager->getData<DataType>(active_key);
    auto target_data = data_manager->getData<DataType>(target_key);

    if (!source_data) {
        std::cerr << "copyEntitiesByIds: Source " << typeid(DataType).name() 
                  << " object ('" << active_key << "') not found." << std::endl;
        return 0;
    }
    if (!target_data) {
        std::cerr << "copyEntitiesByIds: Target " << typeid(DataType).name() 
                  << " object ('" << target_key << "') not found." << std::endl;
        return 0;
    }

    std::unordered_set<EntityId> const selected_entity_ids_set(selected_entity_ids.begin(), selected_entity_ids.end());
    std::size_t const total_copied = source_data->copyByEntityIds(*target_data, selected_entity_ids_set, NotifyObservers::Yes);

    return total_copied;
}

#endif // GROUP_FILTER_HELPER_HPP
