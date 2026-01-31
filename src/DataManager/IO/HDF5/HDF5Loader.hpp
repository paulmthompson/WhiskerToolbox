#ifndef DATAMANAGER_IO_HDF5LOADER_HPP
#define DATAMANAGER_IO_HDF5LOADER_HPP

#include "IO/interface/DataLoader.hpp"
#include "IO/interface/IOTypes.hpp"

/**
 * @brief HDF5 data loader implementation
 * 
 * This loader supports loading various data types from HDF5 format files.
 * Currently supports:
 * - MaskData
 * - LineData
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
};

#endif // DATAMANAGER_IO_HDF5LOADER_HPP
