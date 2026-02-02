#ifndef DATAMANAGER_IO_HDF5LOADER_HPP
#define DATAMANAGER_IO_HDF5LOADER_HPP

#include "IO/core/DataLoader.hpp"
#include "IO/core/IOTypes.hpp"

/**
 * @brief HDF5 data loader implementation
 * 
 * This loader supports loading various data types from HDF5 format files.
 * Currently supports:
 * - MaskData
 * - LineData
 * - DigitalEventSeries
 * - AnalogTimeSeries
 */
class HDF5Loader : public DataLoader {
public:
    /**
     * @brief Get the format identifier
     */
    std::string getFormatId() const override;
    
    /**
     * @brief Check if this loader supports the given data type
     */
    bool supportsDataType(IODataType data_type) const override;
    
    /**
     * @brief Load data from HDF5 file
     */
    LoadResult loadData(
        std::string const& file_path,
        IODataType data_type,
        nlohmann::json const& config
    ) const override;

private:
    /**
     * @brief Load MaskData from HDF5 file
     */
    LoadResult loadMaskData(
        std::string const& file_path,
        nlohmann::json const& config
    ) const;
    
    /**
     * @brief Load LineData from HDF5 file
     */
    LoadResult loadLineData(
        std::string const& file_path,
        nlohmann::json const& config
    ) const;

    /**
     * @brief Load DigitalEventSeries from HDF5 file
     * 
     * Loads event data from an HDF5 file where one dataset contains time values
     * and another dataset contains binary (0/1) event indicators. Events are
     * extracted where the indicator is 1.
     * 
     * Required config fields:
     * - time_key: HDF5 dataset path for time values (float64, fractional seconds)
     * - event_key: HDF5 dataset path for event indicators (0 or 1)
     * - scale: Multiplier to convert time to frame indices (e.g., 30000 for 30kHz sampling)
     * 
     * Optional config fields:
     * - scale_divide: If true, divide by scale instead of multiply (default: false)
     */
    LoadResult loadDigitalEventData(
        std::string const& file_path,
        nlohmann::json const& config
    ) const;

    /**
     * @brief Load AnalogTimeSeries from HDF5 file
     * 
     * Loads analog data from an HDF5 file where one dataset contains time values
     * and another dataset contains floating point signal values.
     * 
     * Required config fields:
     * - time_key: HDF5 dataset path for time values (float64, fractional seconds)
     * - value_key: HDF5 dataset path for analog values (float64)
     * 
     * Optional config fields:
     * - scale: Multiplier to convert time to frame indices (default: 1.0)
     * - scale_divide: If true, divide by scale instead of multiply (default: false)
     */
    LoadResult loadAnalogData(
        std::string const& file_path,
        nlohmann::json const& config
    ) const;
};

#endif // DATAMANAGER_IO_HDF5LOADER_HPP
