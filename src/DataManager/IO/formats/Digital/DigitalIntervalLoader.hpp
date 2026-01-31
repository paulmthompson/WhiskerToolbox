#ifndef DIGITAL_INTERVAL_LOADER_HPP
#define DIGITAL_INTERVAL_LOADER_HPP

#include "../../core/LoaderRegistry.hpp"

/**
 * @brief Digital interval format loader for DigitalIntervalSeries data
 * 
 * This loader provides binary and CSV loading capability for DigitalIntervalSeries.
 * It wraps the existing implementations in DigitalTimeSeries/IO/ with the
 * IFormatLoader interface to integrate with the plugin system.
 * 
 * Supported formats:
 * - "uint16": Binary uint16 format with bit extraction and interval detection
 * - "csv": CSV text format with start/end columns
 * - "multi_column_binary": Multi-column binary CSV format for pulse detection
 */
class DigitalIntervalLoader : public IFormatLoader {
public:
    DigitalIntervalLoader() = default;
    ~DigitalIntervalLoader() override = default;

    /**
     * @brief Load DigitalIntervalSeries data from file
     */
    LoadResult load(std::string const& filepath, 
                   IODataType dataType, 
                   nlohmann::json const& config) const override;
    
    /**
     * @brief Save DigitalIntervalSeries data to file
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
     * @brief Load DigitalIntervalSeries from uint16 binary file
     */
    LoadResult loadUint16Binary(std::string const& filepath, 
                               nlohmann::json const& config) const;
    
    /**
     * @brief Load DigitalIntervalSeries from CSV file
     */
    LoadResult loadCSV(std::string const& filepath, 
                      nlohmann::json const& config) const;
    
    /**
     * @brief Load DigitalIntervalSeries from multi-column binary CSV
     */
    LoadResult loadMultiColumnBinary(std::string const& filepath,
                                    nlohmann::json const& config) const;
    
    /**
     * @brief Save DigitalIntervalSeries to CSV file
     */
    LoadResult saveCSV(std::string const& filepath,
                      nlohmann::json const& config,
                      void const* data) const;
};

#endif // DIGITAL_INTERVAL_LOADER_HPP
