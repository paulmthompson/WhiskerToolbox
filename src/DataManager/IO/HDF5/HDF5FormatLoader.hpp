#ifndef HDF5_FORMAT_LOADER_HPP
#define HDF5_FORMAT_LOADER_HPP

#include "../LoaderRegistry.hpp"

/**
 * @brief HDF5 format loader
 * 
 * This loader provides HDF5 loading capability for MaskData and LineData.
 * It wraps the existing HDF5 loading functionality.
 */
class HDF5FormatLoader : public IFormatLoader {
public:
    HDF5FormatLoader() = default;
    ~HDF5FormatLoader() override = default;

    /**
     * @brief Load data from HDF5 file
     */
    LoadResult load(std::string const& filepath, 
                   IODataType dataType, 
                   nlohmann::json const& config) const override;

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
     * @brief Load MaskData from HDF5 file using existing functionality
     */
    LoadResult loadMaskDataHDF5(std::string const& filepath, 
                               nlohmann::json const& config) const;

    /**
     * @brief Load LineData from HDF5 file using existing functionality
     */
    LoadResult loadLineDataHDF5(std::string const& filepath, 
                               nlohmann::json const& config) const;
};

#endif // HDF5_FORMAT_LOADER_HPP
