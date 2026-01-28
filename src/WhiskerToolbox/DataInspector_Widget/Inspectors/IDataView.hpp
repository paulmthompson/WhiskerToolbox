#ifndef IDATA_VIEW_HPP
#define IDATA_VIEW_HPP

/**
 * @file IDataView.hpp
 * @brief Interface for type-specific data view widgets
 * 
 * IDataView defines the common interface that all type-specific view
 * widgets must implement. View widgets are displayed in the Center zone
 * of the DataInspector and provide data tables and visualizations.
 * 
 * ## Responsibilities
 * - Display data in tabular or visual format
 * - Provide frame selection capability
 * - Respond to data changes
 * 
 * ## Lifecycle
 * View widgets share the same DataInspectorState as their corresponding
 * Properties widgets. When the inspected data key changes:
 * 1. setActiveKey() is called with the new key
 * 2. Data callbacks are set up (if needed)
 * 3. updateView() refreshes the display
 * 
 * @see ViewFactory for creating view widgets
 * @see DataInspectorViewWidget for usage
 */

#include "DataManager/DataManager.hpp"  // For DM_DataType

#include <QString>

#include <string>

/**
 * @brief Interface for type-specific data view widgets
 * 
 * All data view widgets must implement this interface to work with
 * DataInspectorViewWidget's dynamic view creation.
 */
class IDataView {
public:
    virtual ~IDataView() = default;

    // =========================================================================
    // Core Interface
    // =========================================================================

    /**
     * @brief Set the active data key to display
     * @param key The data key in DataManager
     * 
     * This method should:
     * 1. Remove callbacks from any previously active data
     * 2. Store the new key
     * 3. Set up callbacks on the new data (if needed)
     * 4. Update the view to show the new data
     */
    virtual void setActiveKey(std::string const & key) = 0;

    /**
     * @brief Remove all callbacks from the currently active data
     * 
     * Called when the view is being destroyed or when switching to
     * a different data key. Must clean up any registered observers.
     */
    virtual void removeCallbacks() = 0;

    /**
     * @brief Update the view to reflect current data state
     * 
     * Called when the underlying data has changed. Should refresh
     * all UI elements (tables, visualizations, etc.) to show current data.
     */
    virtual void updateView() = 0;

    // =========================================================================
    // Type Information
    // =========================================================================

    /**
     * @brief Get the data type this view handles
     * @return The DM_DataType enum value
     */
    [[nodiscard]] virtual DM_DataType getDataType() const = 0;

    /**
     * @brief Get a human-readable name for this view type
     * @return Display name (e.g., "Point Table", "Line Table", "Mask Table")
     */
    [[nodiscard]] virtual QString getTypeName() const = 0;

    /**
     * @brief Get the current active data key
     * @return The currently displayed data key, or empty string if none
     */
    [[nodiscard]] virtual std::string getActiveKey() const = 0;
};

#endif // IDATA_VIEW_HPP
