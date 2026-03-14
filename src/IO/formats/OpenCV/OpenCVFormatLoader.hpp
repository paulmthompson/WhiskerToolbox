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
    LoadResult load(std::string const & filepath,
                    DM_DataType dataType,
                    nlohmann::json const & config) const override;

    /**
     * @brief Save data to image files using OpenCV
     */
    LoadResult save(std::string const & filepath,
                    DM_DataType dataType,
                    nlohmann::json const & config,
                    void const * data) const override;

    /**
     * @brief Check if this loader supports the format/dataType combination
     */
    bool supportsFormat(std::string const & format, DM_DataType dataType) const override;

    /**
     * @brief Return metadata for all save operations this loader supports
     */
    std::vector<SaverInfo> getSaverInfo() const override;

    /**
     * @brief Get loader name for logging
     */
    std::string getLoaderName() const override;

private:
    /**
     * @brief Load MaskData from image files using existing functionality
     */
    static LoadResult loadMaskDataImage(std::string const & filepath,
                                        nlohmann::json const & config);
};

#endif// OPENCV_FORMAT_LOADER_HPP
