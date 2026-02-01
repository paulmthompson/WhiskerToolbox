#ifndef POINT_LOADER_HPP
#define POINT_LOADER_HPP

#include "../../core/LoaderRegistry.hpp"

/**
 * @brief Point format loader for PointData
 * 
 * This loader provides CSV loading capability for PointData.
 * It wraps the existing implementations in Points/IO/ with the
 * IFormatLoader interface to integrate with the plugin system.
 * 
 * Supported formats:
 * - "csv": Simple CSV format with frame/x/y columns
 * - "dlc_csv": DeepLabCut CSV format (returns first bodypart for single-point loading)
 * 
 * @note For DLC files with multiple bodyparts, the DataManager handles
 *       full multi-bodypart extraction through load_multiple_PointData_from_dlc().
 */
class PointLoader : public IFormatLoader {
public:
    PointLoader() = default;
    ~PointLoader() override = default;

    /**
     * @brief Load PointData from file
     * 
     * For DLC CSV files with multiple bodyparts, this returns the first bodypart
     * as a single PointData. The DataManager handler extracts all bodyparts
     * through the legacy multi-bodypart loading path when needed.
     * 
     * @param filepath Path to the point data file
     * @param dataType Should be IODataType::Points
     * @param config JSON configuration with format and loading options
     * @return LoadResult containing loaded PointData or error
     */
    LoadResult load(std::string const& filepath, 
                   IODataType dataType, 
                   nlohmann::json const& config) const override;
    
    /**
     * @brief Save PointData to file
     * 
     * @param filepath Path where to save the file
     * @param dataType Should be IODataType::Points
     * @param config JSON configuration with saving options
     * @param data Pointer to the PointData to save
     * @return LoadResult indicating success/failure
     */
    LoadResult save(std::string const& filepath, 
                   IODataType dataType, 
                   nlohmann::json const& config, 
                   void const* data) const override;

    /**
     * @brief Check if this loader supports the format/dataType combination
     * 
     * Supports:
     * - format="csv" with dataType=Points
     * - format="dlc_csv" with dataType=Points
     */
    bool supportsFormat(std::string const& format, IODataType dataType) const override;

    /**
     * @brief Get loader name for logging
     */
    std::string getLoaderName() const override;

private:
    /**
     * @brief Load PointData from simple CSV file
     */
    LoadResult loadCSV(std::string const& filepath, 
                      nlohmann::json const& config) const;
    
    /**
     * @brief Load PointData from DLC CSV file
     * 
     * Returns the first bodypart as a single PointData.
     */
    LoadResult loadDLC(std::string const& filepath, 
                      nlohmann::json const& config) const;
    
    /**
     * @brief Save PointData to CSV file
     */
    LoadResult saveCSV(std::string const& filepath,
                      nlohmann::json const& config,
                      void const* data) const;
};

#endif // POINT_LOADER_HPP
