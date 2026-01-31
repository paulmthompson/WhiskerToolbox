#ifndef CSV_LOADER_HPP
#define CSV_LOADER_HPP

#include "../LoaderRegistry.hpp"

/**
 * @brief CSV format loader for LineData
 * 
 * This loader provides CSV loading capability and directly creates
 * data objects by linking to the data type libraries.
 */
class CSVLoader : public IFormatLoader {
public:
    CSVLoader() = default;
    ~CSVLoader() override = default;

    /**
     * @brief Load data from CSV file
     */
    LoadResult load(std::string const& filepath, 
                   IODataType dataType, 
                   nlohmann::json const& config) const override;
    
    /**
     * @brief Save data to CSV file
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
     * @brief Load LineData from CSV using existing functionality
     */
    LoadResult loadLineDataCSV(std::string const& filepath, 
                              nlohmann::json const& config) const;
};

#endif // CSV_LOADER_HPP
