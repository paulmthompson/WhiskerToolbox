#ifndef DATA_MANAGER_EXTENSION_H
#define DATA_MANAGER_EXTENSION_H

#include <map>
#include <memory>
#include <optional>
#include <regex>
#include <string>
#include <utility>
#include <variant>

class AnalogDataAdapter;
class DataManager;
class DigitalEventSeries;
class DigitalIntervalDataAdapter;
class IAnalogSource;
class IIntervalSource;
class ILineSource;
class LineDataAdapter;
class PointData;


/**
 * @brief Extension to DataManager that provides the TableView factory interface.
 * 
 * This class extends the existing DataManager with the getAnalogSource factory method
 * as described in the TableView design document. It can create and cache IAnalogSource
 * adapters for both physical data (AnalogTimeSeries) and virtual data (PointData components).
 */
class DataManagerExtension {
public:
    // Variant handle for strongly-typed source adapters resolved from a name
    using SourceHandle = std::variant<
            std::shared_ptr<IAnalogSource>,
            std::shared_ptr<DigitalEventSeries>,
            std::shared_ptr<IIntervalSource>,
            std::shared_ptr<ILineSource>,
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

    /**
     * @brief Clears the adapter cache.
     * 
     * This method clears all cached IAnalogSource adapters. Should be called
     * when the underlying data changes to ensure fresh adapters are created.
     */
    void clearCache();

    /**
     * @brief Gets digital event data by name.
     * 
     * This method provides direct access to DigitalEventSeries objects.
     * 
     * @param name The name of the event data.
     * @return Shared pointer to DigitalEventSeries, or nullptr if not found.
     */
    auto getEventSource(std::string const & name) -> std::shared_ptr<DigitalEventSeries>;

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

    /**
     * @brief Gets point data by name.
     * 
     * This method provides direct access to PointData objects.
     * 
     * @param name The name of the point data.
     * @return Shared pointer to PointData, or nullptr if not found.
     */
    auto getPointData(std::string const & name) -> std::shared_ptr<PointData>;

    /**
     * @brief Resolve a source name to a concrete adapter variant.
     *
     * Tries known source kinds (analog, interval, event, line, point) and returns
     * the first matching adapter wrapped in a variant. Returns std::nullopt if not found.
     */
    auto resolveSource(std::string const & name) -> std::optional<SourceHandle>;

private:
    /**
     * @brief Creates an AnalogDataAdapter for the given name.
     * @param name The name of the AnalogTimeSeries data.
     * @return Shared pointer to the adapter, or nullptr if not found.
     */
    auto createAnalogDataAdapter(std::string const & name) -> std::shared_ptr<IAnalogSource>;

    /**
     * @brief Creates and retrieves DigitalEventSeries for the given name.
     * @param name The name of the DigitalEventSeries data.
     * @return Shared pointer to DigitalEventSeries, or nullptr if not found.
     */
    auto createDigitalEventSeries(std::string const & name) -> std::shared_ptr<DigitalEventSeries>;

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
     * @brief Creates and retrieves PointData for the given name.
     * @param name The name of the PointData.
     * @return Shared pointer to PointData, or nullptr if not found.
     */
    auto createPointData(std::string const & name) -> std::shared_ptr<PointData>;

    DataManager & m_dataManager;

    // Cache for adapter objects to ensure reuse and correct lifetime
    mutable std::map<std::string, std::shared_ptr<IAnalogSource>> m_dataSourceCache;
    mutable std::map<std::string, std::shared_ptr<DigitalEventSeries>> m_eventSourceCache;
    mutable std::map<std::string, std::shared_ptr<IIntervalSource>> m_intervalSourceCache;
    mutable std::map<std::string, std::shared_ptr<ILineSource>> m_lineSourceCache;
    mutable std::map<std::string, std::shared_ptr<PointData>> m_pointDataCache;
};

#endif// DATA_MANAGER_EXTENSION_H
