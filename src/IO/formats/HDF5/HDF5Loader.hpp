#ifndef DATAMANAGER_IO_HDF5LOADER_HPP
#define DATAMANAGER_IO_HDF5LOADER_HPP

#include "datamanagerio_hdf5_export.h"

#include "DataTypeEnum/DM_DataType.hpp"
#include "IO/core/DataLoader.hpp"

/**
 * @brief HDF5 data loader implementation
 *
 * This loader supports loading various data types from HDF5 format files.
 * Currently supports:
 * - MaskData
 * - LineData
 * - DigitalEventSeries (parallel-array and bit-packed)
 * - DigitalIntervalSeries (bit-packed)
 * - AnalogTimeSeries
 * - TimeFrame (identity layout from dataset shape)
 */
class DATAMANAGERIO_HDF5_EXPORT HDF5Loader : public DataLoader {
public:
    /**
     * @brief Get the format identifier
     */
    std::string getFormatId() const override;

    /**
     * @brief Check if this loader supports the given data type
     */
    bool supportsDataType(DM_DataType data_type) const override;

    /**
     * @brief Load data from HDF5 file
     */
    LoadResult loadData(
            std::string const & file_path,
            DM_DataType data_type,
            nlohmann::json const & config) const override;

private:
    /**
     * @brief Load MaskData from HDF5 file
     */
    static LoadResult loadMaskData(
            std::string const & file_path,
            nlohmann::json const & config) ;

    /**
     * @brief Load LineData from HDF5 file
     */
    static LoadResult loadLineData(
            std::string const & file_path,
            nlohmann::json const & config) ;

    /**
     * @brief Load DigitalEventSeries from HDF5 file
     *
     * Supports two layouts:
     * - Bit-packed (data_key + channel + transition): Wavesurfer-style TTL words
     * - Parallel-array (time_key + event_key): legacy float indicator arrays
     */
    LoadResult loadDigitalEventData(
            std::string const & file_path,
            nlohmann::json const & config) const;

    /**
     * @brief Load DigitalIntervalSeries from bit-packed HDF5 TTL data
     *
     * Required config fields:
     * - data_key: HDF5 dataset path for packed TTL samples
     * - channel: Bit index to extract
     * - transition: "rising" or "falling" for interval start detection
     *
     * Optional config fields:
     * - sweep_row: Row index for 2D datasets (default: 0)
     */
    LoadResult loadDigitalIntervalData(
            std::string const & file_path,
            nlohmann::json const & config) const;

    /**
     * @brief Load identity TimeFrame from HDF5 dataset shape
     *
     * Required config fields:
     * - data_key: HDF5 dataset path used to determine sample count
     * - time_layout: Must be "identity"
     *
     * Optional config fields:
     * - sweep_row: Row index for 2D datasets (default: 0)
     */
    static LoadResult loadIdentityTimeFrameData(
            std::string const & file_path,
            nlohmann::json const & config) ;

    /**
     * @brief Load AnalogTimeSeries from HDF5 file
     */
    static LoadResult loadAnalogData(
            std::string const & file_path,
            nlohmann::json const & config) ;

    /**
     * @brief Load bit-packed uint16 TTL samples from an HDF5 dataset
     */
    static LoadResult loadBitPackedDigitalEventData(
            std::string const & file_path,
            nlohmann::json const & config) ;

    /**
     * @brief Load bit-packed uint16 TTL intervals from an HDF5 dataset
     */
    static LoadResult loadBitPackedDigitalIntervalData(
            std::string const & file_path,
            nlohmann::json const & config) ;
};

#endif// DATAMANAGER_IO_HDF5LOADER_HPP
