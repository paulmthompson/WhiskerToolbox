#ifndef NUMPY_FORMAT_LOADER_HPP
#define NUMPY_FORMAT_LOADER_HPP

#include "../../core/LoaderRegistry.hpp"

/**
 * @brief Numpy format loader
 * 
 * This loader provides numpy (.npy) loading capability for TensorData.
 * It wraps the existing numpy loading functionality.
 */
class NumpyFormatLoader : public IFormatLoader {
public:
    NumpyFormatLoader() = default;
    ~NumpyFormatLoader() override = default;

    /**
     * @brief Load data from .npy file
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
     * @brief Load TensorData from .npy file using existing functionality
     */
    LoadResult loadTensorDataNumpy(std::string const& filepath, 
                                   nlohmann::json const& config) const;
};

#endif // NUMPY_FORMAT_LOADER_HPP
