#ifndef HDF5_FORMAT_LOADER_HPP
#define HDF5_FORMAT_LOADER_HPP

#include "datamanagerio_hdf5_export.h"

#include "../../core/IFormatLoader.hpp"

/**
 * @brief HDF5 format loader
 *
 * This loader provides HDF5 loading capability for MaskData, LineData,
 * DigitalEventSeries, DigitalIntervalSeries, AnalogTimeSeries, and TimeFrame.
 */
class DATAMANAGERIO_HDF5_EXPORT HDF5FormatLoader : public IFormatLoader {
public:
    HDF5FormatLoader() = default;
    ~HDF5FormatLoader() override = default;

    /**
     * @brief Load data from HDF5 file
     */
    LoadResult load(std::string const & filepath,
                    DM_DataType dataType,
                    nlohmann::json const & config) const override;

    /**
     * @brief Check if this loader supports the format/dataType combination
     */
    bool supportsFormat(std::string const & format, DM_DataType dataType) const override;

    /**
     * @brief Get loader name for logging
     */
    std::string getLoaderName() const override;

private:
    /**
     * @brief Load MaskData from HDF5 file using existing functionality
     */
    static LoadResult loadMaskDataHDF5(std::string const & filepath,
                                nlohmann::json const & config) ;

    /**
     * @brief Load LineData from HDF5 file using existing functionality
     */
    static LoadResult loadLineDataHDF5(std::string const & filepath,
                                nlohmann::json const & config) ;

    /**
     * @brief Load DigitalEventSeries from HDF5 file
     */
    static LoadResult loadDigitalEventDataHDF5(std::string const & filepath,
                                        nlohmann::json const & config) ;

    /**
     * @brief Load DigitalIntervalSeries from HDF5 file
     */
    static LoadResult loadDigitalIntervalDataHDF5(std::string const & filepath,
                                           nlohmann::json const & config) ;

    /**
     * @brief Load identity TimeFrame from HDF5 dataset shape
     */
    static LoadResult loadIdentityTimeFrameHDF5(std::string const & filepath,
                                         nlohmann::json const & config) ;

    /**
     * @brief Load AnalogTimeSeries from HDF5 file
     */
    static LoadResult loadAnalogDataHDF5(std::string const & filepath,
                                  nlohmann::json const & config) ;
};

#endif// HDF5_FORMAT_LOADER_HPP
