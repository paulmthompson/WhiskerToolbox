#ifndef DATAMANAGER_IO_OPENCVLOADER_HPP
#define DATAMANAGER_IO_OPENCVLOADER_HPP

#include "IO/core/DataLoader.hpp"
#include "IO/core/IOTypes.hpp"

/**
 * @brief OpenCV data loader implementation
 * 
 * This loader supports loading various data types from image files using OpenCV.
 * Currently supports:
 * - MaskData from binary images (PNG, JPG, BMP, TIFF, etc.)
 */
class OpenCVLoader : public DataLoader {
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
     * @brief Load data from image files using OpenCV
     */
    LoadResult loadData(
        std::string const& file_path,
        IODataType data_type,
        nlohmann::json const& config
    ) const override;

private:
    /**
     * @brief Load MaskData from binary image files
     */
    LoadResult loadMaskData(
        std::string const& file_path,
        nlohmann::json const& config
    ) const;
};

#endif // DATAMANAGER_IO_OPENCVLOADER_HPP
