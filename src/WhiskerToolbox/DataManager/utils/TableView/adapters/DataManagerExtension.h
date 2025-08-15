#ifndef DATA_MANAGER_EXTENSION_H
#define DATA_MANAGER_EXTENSION_H

#include "utils/TableView/adapters/PointComponentAdapter.h"

#include <map>
#include <memory>
#include <regex>
#include <string>
#include <utility>

class AnalogDataAdapter;
class DataManager;
class DigitalEventDataAdapter;
class DigitalIntervalDataAdapter;
class IAnalogSource;
class IEventSource;
class IIntervalSource;
class ILineSource;
class LineDataAdapter;


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
    explicit DataManagerExtension(DataManager & dataManager);

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
    auto getAnalogSource(std::string const & name) -> std::shared_ptr<IAnalogSource>;

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
    auto getEventSource(std::string const & name) -> std::shared_ptr<IEventSource>;

    /**
     * @brief Gets an interval source by name.
     * 
     * This method provides access to IIntervalSource implementations for
     * interval data such as digital interval series.
     * 
     * @param name The name of the interval source.
     * @return Shared pointer to IIntervalSource, or nullptr if not found.
     */
    auto getIntervalSource(std::string const & name) -> std::shared_ptr<IIntervalSource>;

    /**
     * @brief Gets a line source by name.
     * 
     * This method provides access to ILineSource implementations for
     * line data such as LineData.
     * 
     * @param name The name of the line source.
     * @return Shared pointer to ILineSource, or nullptr if not found.
     */
    auto getLineSource(std::string const & name) -> std::shared_ptr<ILineSource>;

private:
    /**
     * @brief Creates an AnalogDataAdapter for the given name.
     * @param name The name of the AnalogTimeSeries data.
     * @return Shared pointer to the adapter, or nullptr if not found.
     */
    auto createAnalogDataAdapter(std::string const & name) -> std::shared_ptr<IAnalogSource>;

    /**
     * @brief Creates a DigitalEventDataAdapter for the given name.
     * @param name The name of the DigitalEventSeries data.
     * @return Shared pointer to the adapter, or nullptr if not found.
     */
    auto createDigitalEventDataAdapter(std::string const & name) -> std::shared_ptr<IEventSource>;

    /**
     * @brief Creates a DigitalIntervalDataAdapter for the given name.
     * @param name The name of the DigitalIntervalSeries data.
     * @return Shared pointer to the adapter, or nullptr if not found.
     */
    auto createDigitalIntervalDataAdapter(std::string const & name) -> std::shared_ptr<IIntervalSource>;

    /**
     * @brief Creates a LineDataAdapter for the given name.
     * @param name The name of the LineData.
     * @return Shared pointer to the adapter, or nullptr if not found.
     */
    auto createLineDataAdapter(std::string const & name) -> std::shared_ptr<ILineSource>;

    /**
     * @brief Creates a PointComponentAdapter for the given name and component.
     * @param pointDataName The name of the PointData.
     * @param component The component type (X or Y).
     * @return Shared pointer to the adapter, or nullptr if not found.
     */
    auto createPointComponentAdapter(std::string const & pointDataName,
                                     PointComponentAdapter::Component component) -> std::shared_ptr<IAnalogSource>;

    /**
     * @brief Parses a virtual source name to extract data name and component.
     * @param name The virtual source name (e.g., "MyPoints.x").
     * @return Pair of (data_name, component) if valid, empty pair otherwise.
     */
    auto parseVirtualSourceName(std::string const & name) -> std::pair<std::string, PointComponentAdapter::Component>;

    DataManager & m_dataManager;

    // Cache for adapter objects to ensure reuse and correct lifetime
    mutable std::map<std::string, std::shared_ptr<IAnalogSource>> m_dataSourceCache;
    mutable std::map<std::string, std::shared_ptr<IEventSource>> m_eventSourceCache;
    mutable std::map<std::string, std::shared_ptr<IIntervalSource>> m_intervalSourceCache;
    mutable std::map<std::string, std::shared_ptr<ILineSource>> m_lineSourceCache;

    // Regex for parsing virtual source names
    static std::regex const s_virtualSourceRegex;
};

#endif// DATA_MANAGER_EXTENSION_H
