#ifndef DIGITAL_EVENT_LOADER_HPP
#define DIGITAL_EVENT_LOADER_HPP

#include "../../core/LoaderRegistry.hpp"

/**
 * @brief Digital event format loader for DigitalEventSeries data
 * 
 * This loader provides binary and CSV loading capability for DigitalEventSeries.
 * It wraps the existing implementations in DigitalTimeSeries/IO/ with the
 * IFormatLoader interface to integrate with the plugin system.
 * 
 * Supported formats:
 * - "uint16": Binary uint16 format with bit extraction
 * - "csv": CSV text format (single or multi-column with identifiers)
 */
class DigitalEventLoader : public IFormatLoader {
public:
    DigitalEventLoader() = default;
    ~DigitalEventLoader() override = default;

    /**
     * @brief Load DigitalEventSeries data from file
     * 
     * For multi-series CSV files (with identifier column), this returns the first
     * series. The DataManager handler should use legacy loading for full support
     * of multiple series per file.
     */
    LoadResult load(std::string const& filepath, 
                   IODataType dataType, 
                   nlohmann::json const& config) const override;
    
    /**
     * @brief Save DigitalEventSeries data to file
     */
    LoadResult save(std::string const& filepath, 
                   IODataType dataType, 
                   nlohmann::json const& config, 
                   void const* data) const override;

    /**
     * @brief Check if this loader supports the format/dataType combination
     */
    bool supportsFormat(std::string const& format, IODataType dataType) const override;

    /**
     * @brief Get loader name for logging
     */
    std::string getLoaderName() const override;

private:
    /**
     * @brief Load DigitalEventSeries from uint16 binary file
     */
    LoadResult loadUint16Binary(std::string const& filepath, 
                               nlohmann::json const& config) const;
    
    /**
     * @brief Load DigitalEventSeries from CSV file
     */
    LoadResult loadCSV(std::string const& filepath, 
                      nlohmann::json const& config) const;
    
    /**
     * @brief Save DigitalEventSeries to CSV file
     */
    LoadResult saveCSV(std::string const& filepath,
                      nlohmann::json const& config,
                      void const* data) const;
};

#endif // DIGITAL_EVENT_LOADER_HPP
