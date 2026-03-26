/**
 * @file IFormatLoader.hpp
 * @brief Interface for format-specific data loaders and batch loading result type.
 *
 * This header is intentionally free of shared-library export macros so that it
 * can be included by both DataManagerIO and its format-plugin shared libraries
 * without creating a circular dependency on the DataManagerIO export header.
 */
#ifndef DATAMANAGER_IO_IFORMATLOADER_HPP
#define DATAMANAGER_IO_IFORMATLOADER_HPP

#include "DataLoader.hpp"
#include "SaverInfo.hpp"

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
    std::vector<LoadResult> results;///< One LoadResult per loaded data object
    bool success = false;           ///< True if at least one object was loaded successfully
    std::string error_message;      ///< Error message if loading failed

    /** @brief Default constructor */
    BatchLoadResult() = default;

    /**
     * @brief Create an error result
     * @param msg Error message describing the failure
     * @return BatchLoadResult with success=false and error message set
     */
    static BatchLoadResult error(std::string const & msg) {
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
        for (auto const & r: results) {
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
    virtual LoadResult load(std::string const & filepath,
                            DM_DataType dataType,
                            nlohmann::json const & config) const = 0;

    /**
     * @brief Check if this loader supports batch loading for the given format/type
     *
     * Batch loading allows a single file to produce multiple data objects.
     * Examples: multi-channel binary files, multi-series CSV files, DLC files
     * with multiple bodyparts.
     *
     * @param format Format string (e.g., "binary", "csv")
     * @param dataType Type of data (e.g., DM_DataType::Analog)
     * @return true if loadBatch() can return multiple objects for this combination
     */
    virtual bool supportsBatchLoading([[maybe_unused]] std::string const & format,
                                      [[maybe_unused]] DM_DataType dataType) const {
        return false;// Default: no batch support
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
    virtual BatchLoadResult loadBatch(std::string const & filepath,
                                      DM_DataType dataType,
                                      nlohmann::json const & config) const {
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
    virtual LoadResult save(std::string const & /*filepath*/,
                            DM_DataType /*dataType*/,
                            nlohmann::json const & /*config*/,
                            void const * /*data*/) const {
        return LoadResult("Saving not supported by this loader: " + getLoaderName());
    }

    /**
     * @brief Check if this loader supports the given format and data type
     *
     * @param format Format string (e.g., "csv", "capnp", "hdf5")
     * @param dataType Type of data (e.g., DM_DataType::Line, DM_DataType::Points)
     * @return true if this loader can handle the format/dataType combination
     */
    virtual bool supportsFormat(std::string const & format, DM_DataType dataType) const = 0;

    /**
     * @brief Get the name of this loader (for logging/debugging)
     */
    virtual std::string getLoaderName() const = 0;

    /**
     * @brief Return metadata for all save operations this loader supports
     *
     * Subclasses that implement save() should override this to advertise
     * their capabilities with typed parameter schemas.
     *
     * @return Vector of SaverInfo (one per supported format/dataType save combination).
     *         Empty by default.
     */
    virtual std::vector<SaverInfo> getSaverInfo() const { return {}; }
};

#endif// DATAMANAGER_IO_IFORMATLOADER_HPP
