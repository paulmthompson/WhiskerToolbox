#ifndef VIEW_FACTORY_HPP
#define VIEW_FACTORY_HPP

/**
 * @file ViewFactory.hpp
 * @brief Factory for creating type-specific data view widgets
 * 
 * ViewFactory provides static methods for creating the appropriate
 * view widget based on data type. This centralizes view creation
 * for the DataInspectorViewWidget.
 * 
 * View widgets display data in tabular or visual formats in the Center zone.
 * 
 * @see IDataView for the interface all views implement
 * @see DataInspectorViewWidget for usage
 */

#include "DataManager/DataManager.hpp"  // For DM_DataType

#include <memory>

class BaseDataView;
class DataManager;
class QWidget;

/**
 * @brief Factory for creating type-specific view widgets
 * 
 * Provides methods to create the appropriate view widget based on data type.
 */
class ViewFactory {
public:
    /**
     * @brief Create a view widget for the given data type
     * @param type The data type to create a view for
     * @param data_manager Shared DataManager for data access
     * @param parent Parent widget
     * @return Unique pointer to the created view, or nullptr if type not supported
     * 
     * The returned view implements IDataView and can be used
     * with DataInspectorViewWidget.
     */
    static std::unique_ptr<BaseDataView> createView(
        DM_DataType type,
        std::shared_ptr<DataManager> data_manager,
        QWidget * parent = nullptr);

    /**
     * @brief Check if a data type has a supported view
     * @param type The data type to check
     * @return true if the type has a view, false otherwise
     */
    static bool hasView(DM_DataType type);
};

#endif // VIEW_FACTORY_HPP
