#ifndef IDATA_INSPECTOR_HPP
#define IDATA_INSPECTOR_HPP

/**
 * @file IDataInspector.hpp
 * @brief Interface for type-specific data inspectors
 * 
 * IDataInspector defines the common interface that all type-specific inspector
 * widgets must implement. This allows the DataInspectorPropertiesWidget to
 * dynamically create and manage inspectors based on data type.
 * 
 * ## Responsibilities
 * - Provide type information for the inspector
 * - Manage active data key and callbacks
 * - Update the view when data changes
 * 
 * ## Implementation
 * Concrete inspectors should inherit from BaseInspector which provides
 * common functionality and implements this interface.
 * 
 * @see BaseInspector for the base implementation
 * @see DataInspectorPropertiesWidget for usage
 */

#include "DataManager/DataManager.hpp"  // For DM_DataType

#include <QString>

#include <string>

/**
 * @brief Interface for type-specific data inspectors
 * 
 * All data inspector widgets must implement this interface to work with
 * the DataInspectorPropertiesWidget's dynamic inspector creation.
 */
class IDataInspector {
public:
    virtual ~IDataInspector() = default;

    // =========================================================================
    // Core Interface
    // =========================================================================

    /**
     * @brief Set the active data key to inspect
     * @param key The data key in DataManager
     * 
     * This method should:
     * 1. Remove callbacks from any previously active data
     * 2. Store the new key
     * 3. Set up callbacks on the new data
     * 4. Update the view to show the new data
     */
    virtual void setActiveKey(std::string const & key) = 0;

    /**
     * @brief Remove all callbacks from the currently active data
     * 
     * Called when the inspector is being destroyed or when switching to
     * a different data key. Must clean up any registered observers.
     */
    virtual void removeCallbacks() = 0;

    /**
     * @brief Update the view to reflect current data state
     * 
     * Called when the underlying data has changed. Should refresh
     * all UI elements (tables, labels, etc.) to show current data.
     */
    virtual void updateView() = 0;

    // =========================================================================
    // Type Information
    // =========================================================================

    /**
     * @brief Get the data type this inspector handles
     * @return The DM_DataType enum value
     */
    [[nodiscard]] virtual DM_DataType getDataType() const = 0;

    /**
     * @brief Get a human-readable name for this inspector type
     * @return Display name (e.g., "Point", "Line", "Mask")
     */
    [[nodiscard]] virtual QString getTypeName() const = 0;

    /**
     * @brief Get the current active data key
     * @return The currently inspected data key, or empty string if none
     */
    [[nodiscard]] virtual std::string getActiveKey() const = 0;

    // =========================================================================
    // Optional Features
    // =========================================================================

    /**
     * @brief Check if this inspector supports export functionality
     * @return true if export is supported, false otherwise
     * 
     * Default returns false. Override in inspectors that support data export.
     */
    [[nodiscard]] virtual bool supportsExport() const { return false; }

    /**
     * @brief Check if this inspector supports group filtering
     * @return true if group filtering is supported, false otherwise
     * 
     * Default returns false. Override in inspectors that work with groups.
     */
    [[nodiscard]] virtual bool supportsGroupFiltering() const { return false; }
};

#endif // IDATA_INSPECTOR_HPP
