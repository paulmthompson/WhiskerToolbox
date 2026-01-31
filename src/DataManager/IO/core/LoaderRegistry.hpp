#ifndef LOADER_REGISTRY_HPP
#define LOADER_REGISTRY_HPP

#include "DataLoader.hpp"
#include "IOTypes.hpp"

#include "nlohmann/json.hpp"

#include <memory>
#include <string>
#include <vector>

/**
 * @brief Interface for format-specific loaders
 * 
 * Loaders now directly create data objects by linking to the data type libraries.
 * This removes the need for the DataFactory abstraction.
 */
class IFormatLoader {
public:
    virtual ~IFormatLoader() = default;
    
    /**
     * @brief Load data from file
     * 
     * @param filepath Path to the file to load
     * @param dataType Type of data being loaded
     * @param config JSON configuration for loading
     * @return LoadResult containing loaded data or error
     */
    virtual LoadResult load(std::string const& filepath, 
                           IODataType dataType, 
                           nlohmann::json const& config) const = 0;
    
    /**
     * @brief Save data to file (optional - not all loaders support saving)
     * 
     * @param filepath Path where to save the file
     * @param dataType Type of data being saved
     * @param config JSON configuration for saving
     * @param data Pointer to the data object to save
     * @return LoadResult indicating success/failure (data field unused)
     */
    virtual LoadResult save(std::string const& /*filepath*/,
                           IODataType /*dataType*/,
                           nlohmann::json const& /*config*/,
                           void const* /*data*/) const {
        return LoadResult("Saving not supported by this loader: " + getLoaderName());
    }
    
    /**
     * @brief Check if this loader supports the given format and data type
     * 
     * @param format Format string (e.g., "csv", "capnp", "hdf5")
     * @param dataType Type of data (e.g., IODataType::Line, IODataType::Points)
     * @return true if this loader can handle the format/dataType combination
     */
    virtual bool supportsFormat(std::string const& format, IODataType dataType) const = 0;
    
    /**
     * @brief Get the name of this loader (for logging/debugging)
     */
    virtual std::string getLoaderName() const = 0;
};

/**
 * @brief Registry for managing data format loaders
 */
class LoaderRegistry {
public:
    LoaderRegistry() = default;
    ~LoaderRegistry() = default;
    
    // Non-copyable, non-movable for simplicity
    LoaderRegistry(LoaderRegistry const&) = delete;
    LoaderRegistry& operator=(LoaderRegistry const&) = delete;
    LoaderRegistry(LoaderRegistry&&) = delete;
    LoaderRegistry& operator=(LoaderRegistry&&) = delete;
    
    /**
     * @brief Register a loader plugin
     * 
     * @param loader Unique pointer to the loader implementation
     */
    void registerLoader(std::unique_ptr<IFormatLoader> loader);
    
    /**
     * @brief Try to load data using registered loaders
     * 
     * Attempts to find a suitable loader for the given format and data type.
     * Returns the first successful load result, or failure if no loader can handle it.
     * 
     * @param format Format string from JSON config
     * @param dataType Type of data being loaded
     * @param filepath Path to the file to load
     * @param config Full JSON configuration object
     * @return LoadResult with loaded data or error message
     */
    LoadResult tryLoad(std::string const& format, 
                      IODataType dataType,
                      std::string const& filepath, 
                      nlohmann::json const& config);
    
    /**
     * @brief Try to save data using registered loaders
     * 
     * Attempts to find a suitable loader for the given format and data type that supports saving.
     * Returns the first successful save result, or failure if no loader can handle it.
     * 
     * @param format Format string from JSON config
     * @param dataType Type of data being saved
     * @param filepath Path where to save the file
     * @param config Full JSON configuration object
     * @param data Pointer to the data object to save
     * @return LoadResult indicating success/failure (data field unused)
     */
    LoadResult trySave(std::string const& format, 
                      IODataType dataType,
                      std::string const& filepath, 
                      nlohmann::json const& config, 
                      void const* data);
    
    /**
     * @brief Check if any registered loader supports the given format/dataType
     * 
     * @param format Format string
     * @param dataType Type of data
     * @return true if at least one loader supports this combination
     */
    bool isFormatSupported(std::string const& format, IODataType dataType) const;
    
    /**
     * @brief Get list of all supported formats for a data type
     * 
     * @param dataType Type of data
     * @return Vector of format strings supported for this data type
     */
    std::vector<std::string> getSupportedFormats(IODataType dataType) const;
    
    /**
     * @brief Get singleton instance
     */
    static LoaderRegistry& getInstance();

private:
    std::vector<std::unique_ptr<IFormatLoader>> m_loaders;
};

#endif // LOADER_REGISTRY_HPP