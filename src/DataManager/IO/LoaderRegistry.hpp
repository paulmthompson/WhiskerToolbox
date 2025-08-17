#ifndef DATAMANAGER_IO_LOADERREGISTRY_HPP
#define DATAMANAGER_IO_LOADERREGISTRY_HPP

#include "DataLoader.hpp"
#include "IOTypes.hpp"
#include <memory>
#include <unordered_map>
#include <vector>
#include <functional>

/**
 * @brief Registry for data loader plugins
 * 
 * This singleton manages all registered data loaders and provides
 * lookup functionality to find appropriate loaders for given formats.
 */
class LoaderRegistry {
public:
    /**
     * @brief Get the singleton instance
     */
    static LoaderRegistry& getInstance();
    
    /**
     * @brief Register a data loader
     * 
     * @param loader Unique pointer to the loader implementation
     * @return true if registration was successful, false if format already exists
     */
    bool registerLoader(std::unique_ptr<DataLoader> loader);
    
    /**
     * @brief Find a loader for the given format and data type
     * 
     * @param format_id Format identifier (e.g., "capnp", "hdf5")
     * @param data_type The data type to load
     * @return Pointer to the loader, or nullptr if not found
     */
    DataLoader const* findLoader(std::string const& format_id, IODataType data_type) const;
    
    /**
     * @brief Get all registered format IDs
     */
    std::vector<std::string> getRegisteredFormats() const;
    
    /**
     * @brief Get all data types supported by a format
     */
    std::vector<IODataType> getSupportedDataTypes(std::string const& format_id) const;

private:
    LoaderRegistry() = default;
    
    // Map from format_id to loader instance
    std::unordered_map<std::string, std::unique_ptr<DataLoader>> _loaders;
};

/**
 * @brief Helper class for automatic loader registration
 * 
 * Plugin implementations should create a static instance of this class
 * to automatically register their loader during static initialization.
 */
template<typename LoaderType>
class LoaderRegistration {
public:
    LoaderRegistration() {
        auto& registry = LoaderRegistry::getInstance();
        registry.registerLoader(std::make_unique<LoaderType>());
    }
};

/**
 * @brief Macro to simplify loader registration
 * 
 * Usage in plugin:
 * REGISTER_LOADER(MyCapnProtoLoader);
 */
#define REGISTER_LOADER(LoaderClass) \
    namespace { \
        static LoaderRegistration<LoaderClass> g_##LoaderClass##_registration; \
    }

#endif // DATAMANAGER_IO_LOADERREGISTRY_HPP
