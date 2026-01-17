#ifndef DATA_MANAGER_EXTENSION_H
#define DATA_MANAGER_EXTENSION_H

#include "DataManagerTypes.hpp"
#include "datamanager_export.h"

#include <map>
#include <memory>
#include <optional>
#include <regex>
#include <string>
#include <utility>
#include <variant>

class AnalogDataAdapter;
class DataManager;
class IAnalogSource;



/**
 * @brief Extension to DataManager that provides the TableView factory interface.
 * 
 * This class extends the existing DataManager with the getAnalogSource factory method
 * as described in the TableView design document. It can create and cache IAnalogSource
 * adapters for both physical data (AnalogTimeSeries) and virtual data (PointData components).
 */
class DATAMANAGER_EXPORT DataManagerExtension {
public:
    // Variant handle for strongly-typed source adapters resolved from a name
    using SourceHandle = std::variant<
            std::shared_ptr<IAnalogSource>,
            std::shared_ptr<DigitalEventSeries>,
            std::shared_ptr<DigitalIntervalSeries>,
            std::shared_ptr<LineData>,
            std::shared_ptr<PointData>>;

    /**
     * @brief Constructs a DataManagerExtension.
     * @param dataManager Reference to the underlying DataManager instance.
     */
    explicit DataManagerExtension(DataManager & dataManager);

    /**
     * @brief Unified access point for analog data sources.
     * 
     * This factory method handles:
     * - Physical data: "LFP" -> AnalogTimeSeries via AnalogDataAdapter
     * 
     * Note: Point component extraction (.x/.y) has been removed. Use PointData directly
     * with dedicated computers for component extraction.
     * 
     * @param name The name of the analog data source.
     * @return Shared pointer to IAnalogSource, or nullptr if not found.
     */
    auto getAnalogSource(std::string const & name) -> std::shared_ptr<IAnalogSource>;

    std::shared_ptr<LineData> getLineSource(std::string const & name);

    /**
     * @brief Clears the adapter cache.
     * 
     * This method clears cached IAnalogSource adapters. Should be called
     * when the underlying analog data changes to ensure fresh adapters are created.
     * 
     * Note: Other data types (DigitalEventSeries, DigitalIntervalSeries, LineData, PointData)
     * are accessed directly from DataManager without caching.
     */
    void clearCache();

    /**
     * @brief Resolve a source name to a concrete adapter variant.
     *
     * Tries known source kinds (analog, interval, event, line, point) and returns
     * the first matching adapter wrapped in a variant. Returns std::nullopt if not found.
     */
    auto resolveSource(std::string const & name) -> std::optional<SourceHandle>;

    std::optional<DataTypeVariant> getDataVariant(std::string const & name);

private:
    /**
     * @brief Creates an AnalogDataAdapter for the given name.
     * @param name The name of the AnalogTimeSeries data.
     * @return Shared pointer to the adapter, or nullptr if not found.
     */
    auto createAnalogDataAdapter(std::string const & name) -> std::shared_ptr<IAnalogSource>;

    DataManager & m_dataManager;

    // Cache for analog adapter objects to ensure reuse and correct lifetime
    mutable std::map<std::string, std::shared_ptr<IAnalogSource>> m_dataSourceCache;
};

#endif// DATA_MANAGER_EXTENSION_H
