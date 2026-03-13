#ifndef DATAMANAGER_IO_NUMPYLOADER_HPP
#define DATAMANAGER_IO_NUMPYLOADER_HPP

#include "IO/core/DataLoader.hpp"
#include "IO/core/IOTypes.hpp"

/**
 * @brief Numpy data loader implementation
 * 
 * This loader supports loading data types from .npy (numpy) format files.
 * Currently supports:
 * - TensorData (multi-dimensional arrays)
 */
class NumpyLoader : public DataLoader {
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
     * @brief Load data from .npy file
     */
    LoadResult loadData(
        std::string const& file_path,
        IODataType data_type,
        nlohmann::json const& config
    ) const override;

private:
    /**
     * @brief Load TensorData from .npy file
     */
    LoadResult loadTensorData(
        std::string const& file_path,
        nlohmann::json const& config
    ) const;
};

#endif // DATAMANAGER_IO_NUMPYLOADER_HPP
