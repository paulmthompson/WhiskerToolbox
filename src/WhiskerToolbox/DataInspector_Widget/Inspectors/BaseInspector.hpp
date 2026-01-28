#ifndef BASE_INSPECTOR_HPP
#define BASE_INSPECTOR_HPP

/**
 * @file BaseInspector.hpp
 * @brief Base class for type-specific data inspectors
 * 
 * BaseInspector provides common functionality shared by all type-specific
 * inspector widgets. It implements the IDataInspector interface and provides:
 * 
 * - DataManager and GroupManager access
 * - Common signal (frameSelected) for navigation
 * - Active key management
 * - Callback removal helper
 * 
 * ## Usage
 * Concrete inspectors inherit from BaseInspector and implement:
 * - setActiveKey() - Set up data-specific callbacks and UI
 * - updateView() - Refresh UI when data changes
 * - getDataType() - Return the handled DM_DataType
 * - getTypeName() - Return display name
 * 
 * @see IDataInspector for the interface
 */

#include "IDataInspector.hpp"

#include <QWidget>

#include <memory>
#include <string>

class DataManager;
class GroupManager;

/**
 * @brief Base implementation for type-specific data inspectors
 * 
 * Provides common infrastructure for all inspector widgets including
 * DataManager access, GroupManager support, and frame selection signaling.
 */
class BaseInspector : public QWidget, public IDataInspector {
    Q_OBJECT

public:
    /**
     * @brief Construct the base inspector
     * @param data_manager Shared DataManager for data access
     * @param group_manager Optional GroupManager for group features
     * @param parent Parent widget
     */
    explicit BaseInspector(
        std::shared_ptr<DataManager> data_manager,
        GroupManager * group_manager = nullptr,
        QWidget * parent = nullptr);

    ~BaseInspector() override;

    // =========================================================================
    // IDataInspector Interface (partial implementation)
    // =========================================================================

    /**
     * @brief Get the current active data key
     * @return The currently inspected data key
     */
    [[nodiscard]] std::string getActiveKey() const override { return _active_key; }

    /**
     * @brief Check if group filtering is supported
     * @return true if GroupManager is available
     */
    [[nodiscard]] bool supportsGroupFiltering() const override { return _group_manager != nullptr; }

    // =========================================================================
    // Group Manager Support
    // =========================================================================

    /**
     * @brief Set the GroupManager for group-aware features
     * @param group_manager GroupManager instance (can be nullptr)
     */
    void setGroupManager(GroupManager * group_manager);

    /**
     * @brief Get the current GroupManager
     * @return GroupManager pointer, or nullptr if not set
     */
    [[nodiscard]] GroupManager * groupManager() const { return _group_manager; }

signals:
    /**
     * @brief Emitted when user selects a frame to navigate to
     * @param frame_id Frame index to navigate to
     * 
     * Connect this to the main application's frame navigation to allow
     * users to jump to specific frames from the inspector.
     */
    void frameSelected(int frame_id);

protected:
    /**
     * @brief Get the DataManager
     * @return Shared pointer to DataManager
     */
    [[nodiscard]] std::shared_ptr<DataManager> dataManager() const { return _data_manager; }

    /**
     * @brief Helper to remove a callback from data
     * @param key Data key
     * @param callback_id Callback ID to remove
     * 
     * Safe to call even if callback_id is -1 or data doesn't exist.
     */
    void removeCallbackFromData(std::string const & key, int & callback_id);

    /**
     * @brief The currently active data key
     */
    std::string _active_key;

    /**
     * @brief Callback ID for data observer (-1 if not registered)
     */
    int _callback_id{-1};

private:
    std::shared_ptr<DataManager> _data_manager;
    GroupManager * _group_manager{nullptr};
};

#endif // BASE_INSPECTOR_HPP
