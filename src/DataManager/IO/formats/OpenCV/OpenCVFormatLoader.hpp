#ifndef OPENCV_FORMAT_LOADER_HPP
#define OPENCV_FORMAT_LOADER_HPP

#include "../../core/LoaderRegistry.hpp"

/**
 * @brief OpenCV format loader
 * 
 * This loader provides OpenCV-based image loading capability for MaskData.
 * It wraps the existing OpenCV loading functionality.
 */
class OpenCVFormatLoader : public IFormatLoader {
public:
    OpenCVFormatLoader() = default;
    ~OpenCVFormatLoader() override = default;

    /**
     * @brief Load data from image files using OpenCV
     */
    LoadResult load(std::string const& filepath, 
                   IODataType dataType, 
                   nlohmann::json const& config) const override;
    
    /**
     * @brief Save data to image files using OpenCV
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
     * @brief Load MaskData from image files using existing functionality
     */
    LoadResult loadMaskDataImage(std::string const& filepath, 
                                nlohmann::json const& config) const;
};

#endif // OPENCV_FORMAT_LOADER_HPP
