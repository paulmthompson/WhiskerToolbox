#ifndef LOADER_REGISTRY_HPP
#define LOADER_REGISTRY_HPP

#include "DataLoader.hpp"
#include "IOTypes.hpp"

#include "nlohmann/json.hpp"

#include <memory>
#include <string>
#include <utility>
#include <vector>

/**
 * @brief Result from loading multiple data objects from a single file
 * 
 * Used for batch loading operations where a single file may contain multiple
 * data objects (e.g., multi-channel binary files, multi-series CSV files,
 * DLC files with multiple bodyparts).
 */
struct BatchLoadResult {
    std::vector<LoadResult> results;  ///< One LoadResult per loaded data object
    bool success = false;             ///< True if at least one object was loaded successfully
    std::string error_message;        ///< Error message if loading failed
    
    /** @brief Default constructor */
    BatchLoadResult() = default;
    
    /**
     * @brief Create an error result
     * @param msg Error message describing the failure
     * @return BatchLoadResult with success=false and error message set
     */
    static BatchLoadResult error(std::string const& msg) {
        BatchLoadResult batch;
        batch.success = false;
        batch.error_message = msg;
        return batch;
    }
    
    /**
     * @brief Create a successful result from a vector of LoadResults
     * @param results Vector of individual LoadResults (moved)
     * @return BatchLoadResult with success=true if results is non-empty
     */
    static BatchLoadResult fromVector(std::vector<LoadResult> results) {
        BatchLoadResult batch;
        batch.results = std::move(results);
        batch.success = !batch.results.empty();
        return batch;
    }
    
    /**
     * @brief Get the number of successfully loaded data objects
     * @return Count of results with success=true
     */
    [[nodiscard]] size_t successCount() const {
        size_t count = 0;
        for (auto const& r : results) {
            if (r.success) {
                ++count;
            }
        }
        return count;
    }
};

/**
 * @brief Interface for format-specific loaders
 * 
 * Loaders now directly create data objects by linking to the data type libraries.
 * This removes the need for the DataFactory abstraction.
 * 
 * The interface supports both single-object loading via load() and multi-object
 * batch loading via loadBatch(). Loaders should override supportsBatchLoading()
 * to indicate when they can return multiple objects from a single file.
 */
class IFormatLoader {
public:
    virtual ~IFormatLoader() = default;
    
    /**
     * @brief Load a single data object from file
     * 
     * For files containing multiple data objects (e.g., multi-channel binary),
     * this method returns only the first object. Use loadBatch() for full
     * multi-object support.
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
     * @brief Check if this loader supports batch loading for the given format/type
     * 
     * Batch loading allows a single file to produce multiple data objects.
     * Examples: multi-channel binary files, multi-series CSV files, DLC files
     * with multiple bodyparts.
     * 
     * @param format Format string (e.g., "binary", "csv")
     * @param dataType Type of data (e.g., IODataType::Analog)
     * @return true if loadBatch() can return multiple objects for this combination
     */
    virtual bool supportsBatchLoading([[maybe_unused]] std::string const& format, 
                                      [[maybe_unused]] IODataType dataType) const {
        return false;  // Default: no batch support
    }
    
    /**
     * @brief Load multiple data objects from a single file
     * 
     * This method is used for files that contain multiple data objects,
     * such as multi-channel binary recordings or CSV files with multiple series.
     * 
     * The default implementation wraps load() in a BatchLoadResult.
     * Loaders that support multi-object files should override this method
     * and return all objects in the file.
     * 
     * @param filepath Path to the file to load
     * @param dataType Type of data being loaded
     * @param config JSON configuration for loading
     * @return BatchLoadResult containing all loaded data objects
     */
    virtual BatchLoadResult loadBatch(std::string const& filepath,
                                      IODataType dataType,
                                      nlohmann::json const& config) const {
        // Default implementation: wrap single load in BatchLoadResult
        auto result = load(filepath, dataType, config);
        if (result.success) {
            return BatchLoadResult::fromVector({std::move(result)});
        }
        return BatchLoadResult::error(result.error_message);
    }
    
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
     * @brief Try to load multiple data objects from a single file using batch loading
     * 
     * This method is the preferred way to load files that may contain multiple
     * data objects (e.g., multi-channel binary recordings, multi-series CSV files).
     * 
     * If the loader supports batch loading (supportsBatchLoading() returns true),
     * it will use loadBatch() to get all objects. Otherwise, it falls back to
     * single-object load() wrapped in a BatchLoadResult.
     * 
     * @param format Format string from JSON config
     * @param dataType Type of data being loaded
     * @param filepath Path to the file to load
     * @param config Full JSON configuration object
     * @return BatchLoadResult with all loaded data objects or error message
     */
    BatchLoadResult tryLoadBatch(std::string const& format, 
                                 IODataType dataType,
                                 std::string const& filepath, 
                                 nlohmann::json const& config);
    
    /**
     * @brief Check if batch loading is supported for the given format/dataType
     * 
     * @param format Format string
     * @param dataType Type of data
     * @return true if at least one loader supports batch loading for this combination
     */
    bool isBatchLoadingSupported(std::string const& format, IODataType dataType) const;
    
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