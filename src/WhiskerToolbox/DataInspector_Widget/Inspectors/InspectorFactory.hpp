#ifndef INSPECTOR_FACTORY_HPP
#define INSPECTOR_FACTORY_HPP

/**
 * @file InspectorFactory.hpp
 * @brief Factory for creating type-specific data inspectors
 * 
 * InspectorFactory provides static methods for creating the appropriate
 * inspector widget based on data type. This centralizes inspector creation
 * and makes it easy to add new inspector types.
 * 
 * @see IDataInspector for the interface all inspectors implement
 * @see DataInspectorPropertiesWidget for usage
 */

#include "DataManager/DataManager.hpp"  // For DM_DataType

#include <memory>

class BaseInspector;
class DataManager;
class GroupManager;
class QWidget;

/**
 * @brief Factory for creating type-specific inspector widgets
 * 
 * Provides methods to create the appropriate inspector based on data type.
 */
class InspectorFactory {
public:
    /**
     * @brief Create an inspector for the given data type
     * @param type The data type to create an inspector for
     * @param data_manager Shared DataManager for data access
     * @param group_manager Optional GroupManager for group features
     * @param parent Parent widget
     * @return Unique pointer to the created inspector, or nullptr if type not supported
     * 
     * The returned inspector implements IDataInspector and can be used
     * with DataInspectorPropertiesWidget.
     */
    static std::unique_ptr<BaseInspector> createInspector(
        DM_DataType type,
        std::shared_ptr<DataManager> data_manager,
        GroupManager * group_manager = nullptr,
        QWidget * parent = nullptr);

    /**
     * @brief Check if a data type has a supported inspector
     * @param type The data type to check
     * @return true if an inspector exists for this type
     */
    static bool hasInspector(DM_DataType type);

private:
    // Non-instantiable factory class
    InspectorFactory() = delete;
};

#endif // INSPECTOR_FACTORY_HPP
