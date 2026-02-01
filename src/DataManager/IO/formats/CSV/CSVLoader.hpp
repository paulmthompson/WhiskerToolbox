#ifndef CSV_FORMAT_LOADER_HPP
#define CSV_FORMAT_LOADER_HPP

#include "../../core/LoaderRegistry.hpp"

/**
 * @brief Format-centric CSV loader for all CSV file data types
 * 
 * This loader follows the format-centric architecture where one loader
 * handles a specific file format (CSV) for all applicable data types.
 * 
 * Supported data types:
 * - IODataType::Line: Line/whisker data (single or multi-file CSV)
 * - IODataType::Points: Point tracking data (simple CSV or DLC format)
 * - IODataType::Analog: Analog time series (single/two column CSV)
 * - IODataType::DigitalEvent: Digital event timestamps (with optional multi-series)
 * - IODataType::DigitalInterval: Digital intervals (start/end column pairs)
 * 
 * This loader supports batch loading for:
 * - DigitalEvent CSV with identifier column (returns one series per identifier)
 * - Points DLC format with all_bodyparts=true (returns one PointData per bodypart)
 * 
 * Configuration options vary by data type - see individual load methods
 * for required and optional JSON configuration fields.
 */
class CSVLoader : public IFormatLoader {
public:
    CSVLoader() = default;
    ~CSVLoader() override = default;

    /**
     * @brief Load a single data object from CSV file
     * 
     * For multi-series CSV files (DigitalEvent with identifier, DLC with multiple bodyparts),
     * this returns only the first object. Use loadBatch() for all objects.
     */
    LoadResult load(std::string const& filepath, 
                   IODataType dataType, 
                   nlohmann::json const& config) const override;
    
    /**
     * @brief Check if batch loading is supported for this format/type
     * 
     * Returns true for:
     * - DigitalEvent with identifier column
     * - Points with DLC format and all_bodyparts=true
     */
    bool supportsBatchLoading(std::string const& format, 
                              IODataType dataType) const override;
    
    /**
     * @brief Load all data objects from a multi-series CSV file
     * 
     * For DigitalEvent, returns one DigitalEventSeries per unique identifier.
     * For Points with DLC format, returns one PointData per bodypart.
     */
    BatchLoadResult loadBatch(std::string const& filepath,
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
     * 
     * Supports format "csv" for Line, Points, Analog, DigitalEvent, DigitalInterval
     * Supports format "dlc_csv" for Points (DLC/DeepLabCut format - legacy compatibility)
     */
    bool supportsFormat(std::string const& format, IODataType dataType) const override;

    /**
     * @brief Get loader name for logging
     */
    std::string getLoaderName() const override;

private:
    /**
     * @brief Load LineData from CSV (single or multi-file)
     */
    LoadResult loadLineDataCSV(std::string const& filepath, 
                              nlohmann::json const& config) const;
    
    /**
     * @brief Load PointData from simple CSV format
     */
    LoadResult loadPointDataCSV(std::string const& filepath, 
                               nlohmann::json const& config) const;
    
    /**
     * @brief Load PointData from DLC format CSV
     */
    LoadResult loadPointDataDLC(std::string const& filepath, 
                               nlohmann::json const& config) const;
    
    /**
     * @brief Load all bodyparts from DLC format CSV
     * @return BatchLoadResult with one LoadResult per bodypart
     */
    BatchLoadResult loadPointDataDLCBatch(std::string const& filepath, 
                                          nlohmann::json const& config) const;
    
    /**
     * @brief Load AnalogTimeSeries from CSV
     */
    LoadResult loadAnalogCSV(std::string const& filepath, 
                            nlohmann::json const& config) const;
    
    /**
     * @brief Load DigitalEventSeries from CSV (single series)
     */
    LoadResult loadDigitalEventCSV(std::string const& filepath, 
                                  nlohmann::json const& config) const;
    
    /**
     * @brief Load all DigitalEventSeries from CSV with identifiers
     * @return BatchLoadResult with one LoadResult per unique identifier
     */
    BatchLoadResult loadDigitalEventCSVBatch(std::string const& filepath, 
                                             nlohmann::json const& config) const;
    
    /**
     * @brief Load DigitalIntervalSeries from CSV
     */
    LoadResult loadDigitalIntervalCSV(std::string const& filepath, 
                                     nlohmann::json const& config) const;
    
    /**
     * @brief Save LineData to CSV
     */
    LoadResult saveLineDataCSV(std::string const& filepath,
                              nlohmann::json const& config,
                              void const* data) const;
    
    /**
     * @brief Save PointData to CSV
     */
    LoadResult savePointDataCSV(std::string const& filepath,
                               nlohmann::json const& config,
                               void const* data) const;
    
    /**
     * @brief Save AnalogTimeSeries to CSV
     */
    LoadResult saveAnalogCSV(std::string const& filepath,
                            nlohmann::json const& config,
                            void const* data) const;
    
    /**
     * @brief Save DigitalEventSeries to CSV
     */
    LoadResult saveDigitalEventCSV(std::string const& filepath,
                                  nlohmann::json const& config,
                                  void const* data) const;
    
    /**
     * @brief Save DigitalIntervalSeries to CSV
     */
    LoadResult saveDigitalIntervalCSV(std::string const& filepath,
                                     nlohmann::json const& config,
                                     void const* data) const;
};

#endif // CSV_FORMAT_LOADER_HPP
