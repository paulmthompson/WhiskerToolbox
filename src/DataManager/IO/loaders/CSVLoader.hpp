#ifndef CSV_LOADER_HPP
#define CSV_LOADER_HPP

#include "../LoaderRegistry.hpp"

/**
 * @brief CSV format loader for LineData
 * 
 * This is an "internal plugin" that provides CSV loading capability
 * without external dependencies. It wraps the existing CSV loading
 * functionality from Lines/IO/CSV.
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
                   nlohmann::json const& config, 
                   DataFactory* factory) const override;

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
                              nlohmann::json const& config, 
                              DataFactory* factory) const;
};

#endif // CSV_LOADER_HPP
