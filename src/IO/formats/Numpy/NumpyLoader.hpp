#ifndef DATAMANAGER_IO_NUMPYLOADER_HPP
#define DATAMANAGER_IO_NUMPYLOADER_HPP

#include "DataTypeEnum/DM_DataType.hpp"
#include "IO/core/DataLoader.hpp"

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
    bool supportsDataType(DM_DataType data_type) const override;

    /**
     * @brief Load data from .npy file
     */
    LoadResult loadData(
            std::string const & file_path,
            DM_DataType data_type,
            nlohmann::json const & config) const override;

private:
    /**
     * @brief Load TensorData from .npy file
     */
    LoadResult loadTensorData(
            std::string const & file_path,
            nlohmann::json const & config) const;
};

#endif// DATAMANAGER_IO_NUMPYLOADER_HPP
