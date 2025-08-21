#ifndef DATAMANAGER_IO_PLUGINLOADER_HPP
#define DATAMANAGER_IO_PLUGINLOADER_HPP

#include "interface/DataLoader.hpp"
#include "interface/IOTypes.hpp"
#include <memory>
#include <nlohmann/json.hpp>
#include <string>

/**
 * @brief High-level interface for loading data using plugins
 * 
 * This class provides a bridge between DataManager and the plugin system,
 * allowing for gradual migration from the existing hardcoded loaders.
 */
class PluginLoader {
public:
    /**
     * @brief Load data using the plugin system
     * 
     * @param file_path Path to the data file
     * @param data_type Type of data to load
     * @param config JSON configuration containing format and other options
     * @param factory Factory for creating data objects (provided by DataManager)
     * @return LoadResult containing the loaded data or error information
     */
    static LoadResult loadData(
        std::string const& file_path,
        IODataType data_type,
        nlohmann::json const& config,
        class DataFactory* factory
    );
    
    /**
     * @brief Check if a format is supported by the plugin system
     * 
     * @param format_id Format identifier (e.g., "capnp", "hdf5")
     * @param data_type Data type to check support for
     * @return true if the format is supported for this data type
     */
    static bool isFormatSupported(std::string const& format_id, IODataType data_type);

    /**
     * @brief Get all formats supported by the plugin system
     */
    static std::vector<std::string> getSupportedFormats();

private:
    // Note: Data setting is now handled by DataManager, not PluginLoader
};

#endif // DATAMANAGER_IO_PLUGINLOADER_HPP
