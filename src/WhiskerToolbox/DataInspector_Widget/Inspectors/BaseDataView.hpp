#ifndef BASE_DATA_VIEW_HPP
#define BASE_DATA_VIEW_HPP

/**
 * @file BaseDataView.hpp
 * @brief Base class for type-specific data view widgets
 * 
 * BaseDataView provides common functionality shared by all type-specific
 * view widgets. It implements the IDataView interface and provides:
 * 
 * - DataManager access
 * - Common signal (frameSelected) for navigation
 * - Active key management
 * 
 * ## Usage
 * Concrete views inherit from BaseDataView and implement:
 * - setActiveKey() - Set up data-specific callbacks and UI
 * - updateView() - Refresh UI when data changes
 * - getDataType() - Return the handled DM_DataType
 * - getTypeName() - Return display name
 * 
 * @see IDataView for the interface
 */

#include "IDataView.hpp"

#include <QWidget>

#include <memory>
#include <string>

class DataManager;

/**
 * @brief Base implementation for type-specific data view widgets
 * 
 * Provides common infrastructure for all view widgets including
 * DataManager access and frame selection signaling.
 */
class BaseDataView : public QWidget, public IDataView {
    Q_OBJECT

public:
    /**
     * @brief Construct the base data view
     * @param data_manager Shared DataManager for data access
     * @param parent Parent widget
     */
    explicit BaseDataView(
        std::shared_ptr<DataManager> data_manager,
        QWidget * parent = nullptr);

    ~BaseDataView() override;

    // =========================================================================
    // IDataView Interface (partial implementation)
    // =========================================================================

    /**
     * @brief Get the current active data key
     * @return The currently displayed data key
     */
    [[nodiscard]] std::string getActiveKey() const override { return _active_key; }

signals:
    /**
     * @brief Emitted when user selects a frame to navigate to
     * @param frame_id Frame index to navigate to
     * 
     * Connect this to the main application's frame navigation to allow
     * users to jump to specific frames from the view.
     */
    void frameSelected(int frame_id);

protected:
    /**
     * @brief Get the DataManager
     * @return Shared pointer to DataManager
     */
    [[nodiscard]] std::shared_ptr<DataManager> dataManager() const { return _data_manager; }

    std::string _active_key;

private:
    std::shared_ptr<DataManager> _data_manager;
};

#endif // BASE_DATA_VIEW_HPP
