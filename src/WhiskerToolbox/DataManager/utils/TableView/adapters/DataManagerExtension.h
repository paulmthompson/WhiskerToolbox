#ifndef DATA_MANAGER_EXTENSION_H
#define DATA_MANAGER_EXTENSION_H

#include "utils/TableView/interfaces/IAnalogSource.h"
#include "utils/TableView/interfaces/IEventSource.h"
#include "utils/TableView/adapters/AnalogDataAdapter.h"
#include "utils/TableView/adapters/PointComponentAdapter.h"
#include "DataManager.hpp"

#include <map>
#include <memory>
#include <string>
#include <regex>
#include <utility>

/**
 * @brief Extension to DataManager that provides the TableView factory interface.
 * 
 * This class extends the existing DataManager with the getAnalogSource factory method
 * as described in the TableView design document. It can create and cache IAnalogSource
 * adapters for both physical data (AnalogTimeSeries) and virtual data (PointData components).
 */
class DataManagerExtension {
public:
    /**
     * @brief Constructs a DataManagerExtension.
     * @param dataManager Reference to the underlying DataManager instance.
     */
    explicit DataManagerExtension(DataManager& dataManager);

    /**
     * @brief Unified access point for all data sources.
     * 
     * This factory method can handle:
     * - Physical data: "LFP" -> AnalogTimeSeries via AnalogDataAdapter
     * - Virtual data: "MyPoints.x" or "MyPoints.y" -> PointData components via PointComponentAdapter
     * 
     * @param name The name of the data source. Can be a direct name for physical data
     *             or "DataName.component" for virtual sources (e.g., "Spikes.x").
     * @return Shared pointer to IAnalogSource, or nullptr if not found.
     */
    auto getAnalogSource(const std::string& name) -> std::shared_ptr<IAnalogSource>;

    /**
     * @brief Clears the adapter cache.
     * 
     * This method clears all cached IAnalogSource adapters. Should be called
     * when the underlying data changes to ensure fresh adapters are created.
     */
    void clearCache();

    /**
     * @brief Gets an event source by name.
     * 
     * This method provides access to IEventSource implementations for
     * discrete event data such as digital event series.
     * 
     * @param name The name of the event source.
     * @return Shared pointer to IEventSource, or nullptr if not found.
     */
    auto getEventSource(const std::string& name) -> std::shared_ptr<IEventSource>;

private:
    /**
     * @brief Creates an AnalogDataAdapter for the given name.
     * @param name The name of the AnalogTimeSeries data.
     * @return Shared pointer to the adapter, or nullptr if not found.
     */
    auto createAnalogDataAdapter(const std::string& name) -> std::shared_ptr<IAnalogSource>;

    /**
     * @brief Creates a PointComponentAdapter for the given name and component.
     * @param pointDataName The name of the PointData.
     * @param component The component type (X or Y).
     * @return Shared pointer to the adapter, or nullptr if not found.
     */
    auto createPointComponentAdapter(const std::string& pointDataName, 
                                   PointComponentAdapter::Component component) -> std::shared_ptr<IAnalogSource>;

    /**
     * @brief Parses a virtual source name to extract data name and component.
     * @param name The virtual source name (e.g., "MyPoints.x").
     * @return Pair of (data_name, component) if valid, empty pair otherwise.
     */
    auto parseVirtualSourceName(const std::string& name) -> std::pair<std::string, PointComponentAdapter::Component>;

    DataManager& m_dataManager;
    
    // Cache for adapter objects to ensure reuse and correct lifetime
    mutable std::map<std::string, std::shared_ptr<IAnalogSource>> m_dataSourceCache;
    mutable std::map<std::string, std::shared_ptr<IEventSource>> m_eventSourceCache;
    
    // Regex for parsing virtual source names
    static const std::regex s_virtualSourceRegex;
};

#endif // DATA_MANAGER_EXTENSION_H
