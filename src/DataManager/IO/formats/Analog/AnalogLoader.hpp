#ifndef ANALOG_LOADER_HPP
#define ANALOG_LOADER_HPP

#include "../../core/LoaderRegistry.hpp"

/**
 * @brief Analog format loader for AnalogTimeSeries data
 * 
 * This loader provides Binary and CSV loading capability for AnalogTimeSeries.
 * It wraps the existing implementations in AnalogTimeSeries/IO/ with the
 * IFormatLoader interface to integrate with the plugin system.
 * 
 * Supported formats:
 * - "binary": Binary file format (int16, float32, int8, uint16, float64)
 * - "csv": CSV text format
 */
class AnalogLoader : public IFormatLoader {
public:
    AnalogLoader() = default;
    ~AnalogLoader() override = default;

    /**
     * @brief Load AnalogTimeSeries data from file
     * 
     * For multi-channel binary files, this returns a single LoadResult containing
     * the first channel. The DataManager.cpp handler extracts all channels from
     * the legacy loader when needed.
     * 
     * @note This loader returns a single AnalogTimeSeries. For multi-channel support,
     *       the DataManager handler should detect this and use legacy loading.
     */
    LoadResult load(std::string const& filepath, 
                   IODataType dataType, 
                   nlohmann::json const& config) const override;
    
    /**
     * @brief Save AnalogTimeSeries data to file
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
     * @brief Load AnalogTimeSeries from binary file
     */
    LoadResult loadBinary(std::string const& filepath, 
                         nlohmann::json const& config) const;
    
    /**
     * @brief Load AnalogTimeSeries from CSV file
     */
    LoadResult loadCSV(std::string const& filepath, 
                      nlohmann::json const& config) const;
    
    /**
     * @brief Save AnalogTimeSeries to CSV file
     */
    LoadResult saveCSV(std::string const& filepath,
                      nlohmann::json const& config,
                      void const* data) const;
};

#endif // ANALOG_LOADER_HPP
