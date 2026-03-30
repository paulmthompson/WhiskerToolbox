#ifndef LOADER_REGISTRY_HPP
#define LOADER_REGISTRY_HPP

#include "datamanagerio_export.h"

#include "IFormatLoader.hpp"

#include <memory>
#include <string>
#include <vector>

/**
 * @brief Registry for managing data format loaders
 */
class DATAMANAGERIO_EXPORT LoaderRegistry {
public:
    LoaderRegistry() = default;
    ~LoaderRegistry() = default;

    // Non-copyable, non-movable for simplicity
    LoaderRegistry(LoaderRegistry const &) = delete;
    LoaderRegistry & operator=(LoaderRegistry const &) = delete;
    LoaderRegistry(LoaderRegistry &&) = delete;
    LoaderRegistry & operator=(LoaderRegistry &&) = delete;

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
    LoadResult tryLoad(std::string const & format,
                       DM_DataType dataType,
                       std::string const & filepath,
                       nlohmann::json const & config);

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
    LoadResult trySave(std::string const & format,
                       DM_DataType dataType,
                       std::string const & filepath,
                       nlohmann::json const & config,
                       void const * data);

    /**
     * @brief Check if any registered loader supports the given format/dataType
     * 
     * @param format Format string
     * @param dataType Type of data
     * @return true if at least one loader supports this combination
     */
    bool isFormatSupported(std::string const & format, DM_DataType dataType) const;

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
    BatchLoadResult tryLoadBatch(std::string const & format,
                                 DM_DataType dataType,
                                 std::string const & filepath,
                                 nlohmann::json const & config);

    /**
     * @brief Check if batch loading is supported for the given format/dataType
     * 
     * @param format Format string
     * @param dataType Type of data
     * @return true if at least one loader supports batch loading for this combination
     */
    bool isBatchLoadingSupported(std::string const & format, DM_DataType dataType) const;

    /**
     * @brief Get list of all supported formats for a data type
     * 
     * @param dataType Type of data
     * @return Vector of format strings supported for this data type
     */
    std::vector<std::string> getSupportedFormats(DM_DataType dataType) const;

    /**
     * @brief Query all registered loader capabilities
     * @return Vector of LoaderInfo from all registered loaders
     */
    [[nodiscard]] std::vector<LoaderInfo> getSupportedLoadFormats() const;

    /**
     * @brief Query registered loader capabilities filtered by data type
     * @param dataType Only return loaders that handle this data type
     * @return Filtered vector of LoaderInfo
     */
    [[nodiscard]] std::vector<LoaderInfo> getSupportedLoadFormats(DM_DataType dataType) const;

    /**
     * @brief Query all registered saver capabilities
     * @return Vector of SaverInfo from all registered loaders
     */
    [[nodiscard]] std::vector<SaverInfo> getSupportedSaveFormats() const;

    /**
     * @brief Query registered saver capabilities filtered by data type
     * @param dataType Only return savers that handle this data type
     * @return Filtered vector of SaverInfo
     */
    [[nodiscard]] std::vector<SaverInfo> getSupportedSaveFormats(DM_DataType dataType) const;

    /**
     * @brief Get singleton instance
     */
    static LoaderRegistry & getInstance();

private:
    std::vector<std::unique_ptr<IFormatLoader>> m_loaders;
};

#endif// LOADER_REGISTRY_HPP