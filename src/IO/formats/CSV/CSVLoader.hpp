#ifndef CSV_FORMAT_LOADER_HPP
#define CSV_FORMAT_LOADER_HPP

#include "datamanagerio_export.h"

#include "../../core/LoaderRegistry.hpp"

/**
 * @brief Format-centric CSV loader for all CSV file data types
 * 
 * This loader follows the format-centric architecture where one loader
 * handles a specific file format (CSV) for all applicable data types.
 * 
 * Supported data types:
 * - DM_DataType::Line: Line/whisker data (single or multi-file CSV)
 * - DM_DataType::Points: Point tracking data (simple CSV or DLC format)
 * - DM_DataType::Mask: Mask data (RLE-encoded CSV)
 * - DM_DataType::Analog: Analog time series (single/two column CSV)
 * - DM_DataType::DigitalEvent: Digital event timestamps (with optional multi-series)
 * - DM_DataType::DigitalInterval: Digital intervals (start/end column pairs or binary state columns)
 * 
 * This loader supports batch loading for:
 * - DigitalEvent CSV with identifier column (returns one series per identifier)
 * - Points DLC format with all_bodyparts=true (returns one PointData per bodypart)
 * - DigitalInterval with csv_layout="binary_state" and all_columns=true (returns one series per column)
 * 
 * CSV Layouts for DigitalInterval:
 * - "intervals" (default): Two columns with start/end times for each interval
 * - "binary_state": Rows represent time points, columns contain 0/1 state values.
 *   Intervals are extracted from contiguous regions where value >= threshold.
 * 
 * Configuration options vary by data type - see individual load methods
 * for required and optional JSON configuration fields.
 */
class DATAMANAGERIO_EXPORT CSVLoader : public IFormatLoader {
public:
    CSVLoader() = default;
    ~CSVLoader() override = default;

    /**
     * @brief Load a single data object from CSV file
     * 
     * For multi-series CSV files (DigitalEvent with identifier, DLC with multiple bodyparts),
     * this returns only the first object. Use loadBatch() for all objects.
     */
    LoadResult load(std::string const & filepath,
                    DM_DataType dataType,
                    nlohmann::json const & config) const override;

    /**
     * @brief Check if batch loading is supported for this format/type
     * 
     * Returns true for:
     * - DigitalEvent with identifier column
     * - Points with DLC format and all_bodyparts=true
     * - DigitalInterval with csv_layout="binary_state" (multiple columns)
     */
    bool supportsBatchLoading(std::string const & format,
                              DM_DataType dataType) const override;

    /**
     * @brief Load all data objects from a multi-series CSV file
     * 
     * For DigitalEvent, returns one DigitalEventSeries per unique identifier.
     * For Points with DLC format, returns one PointData per bodypart.
     * For DigitalInterval with binary_state layout, returns one series per data column.
     */
    BatchLoadResult loadBatch(std::string const & filepath,
                              DM_DataType dataType,
                              nlohmann::json const & config) const override;

    /**
     * @brief Save data to CSV file
     */
    LoadResult save(std::string const & filepath,
                    DM_DataType dataType,
                    nlohmann::json const & config,
                    void const * data) const override;

    /**
     * @brief Check if this loader supports the format/dataType combination
     * 
     * Supports format "csv" for Line, Points, Mask, Analog, DigitalEvent, DigitalInterval
     * Supports format "dlc_csv" for Points (DLC/DeepLabCut format - legacy compatibility)
     */
    bool supportsFormat(std::string const & format, DM_DataType dataType) const override;

    /**
     * @brief Get loader name for logging
     */
    std::string getLoaderName() const override;

    /**
     * @brief Return metadata for all save operations this loader supports
     */
    std::vector<SaverInfo> getSaverInfo() const override;

private:
    /**
     * @brief Load LineData from CSV (single or multi-file)
     */
    static LoadResult loadLineDataCSV(std::string const & filepath,
                                      nlohmann::json const & config);

    /**
     * @brief Load PointData from simple CSV format
     */
    static LoadResult loadPointDataCSV(std::string const & filepath,
                                       nlohmann::json const & config);

    /**
     * @brief Load PointData from DLC format CSV
     */
    static LoadResult loadPointDataDLC(std::string const & filepath,
                                       nlohmann::json const & config);

    /**
     * @brief Load all bodyparts from DLC format CSV
     * @return BatchLoadResult with one LoadResult per bodypart
     */
    static BatchLoadResult loadPointDataDLCBatch(std::string const & filepath,
                                                 nlohmann::json const & config);

    /**
     * @brief Load AnalogTimeSeries from CSV
     */
    static LoadResult loadAnalogCSV(std::string const & filepath,
                                    nlohmann::json const & config);

    /**
     * @brief Load DigitalEventSeries from CSV (single series)
     */
    static LoadResult loadDigitalEventCSV(std::string const & filepath,
                                          nlohmann::json const & config);

    /**
     * @brief Load all DigitalEventSeries from CSV with identifiers
     * @return BatchLoadResult with one LoadResult per unique identifier
     */
    static BatchLoadResult loadDigitalEventCSVBatch(std::string const & filepath,
                                                    nlohmann::json const & config);

    /**
     * @brief Load DigitalIntervalSeries from CSV
     * 
     * Supports two layouts via csv_layout config:
     * - "intervals" (default): Two-column CSV with start/end times
     * - "binary_state": Multi-column CSV where rows are time points and 
     *   cell values represent on/off state (0/1). Intervals extracted from
     *   contiguous "on" regions.
     */
    static LoadResult loadDigitalIntervalCSV(std::string const & filepath,
                                             nlohmann::json const & config);

    /**
     * @brief Load DigitalIntervalSeries from binary state CSV layout
     * 
     * Parses a single data column where rows represent time points and
     * cell values represent binary state (0 or 1). Intervals are extracted
     * from contiguous regions where value >= threshold.
     * 
     * Config options:
     * - header_lines_to_skip: Lines before column headers (default: 5)
     * - time_column: Column index for time values (default: 0)
     * - data_column: Column index for binary state values (default: 1)
     * - delimiter: Column separator (default: "\t")
     * - binary_threshold: Values >= this are "on" (default: 0.5)
     */
    static LoadResult loadDigitalIntervalBinaryState(std::string const & filepath,
                                                     nlohmann::json const & config);

    /**
     * @brief Load all columns from binary state CSV as DigitalIntervalSeries
     * 
     * Returns one DigitalIntervalSeries per data column (excluding time column).
     * Each series is named using the column header from the file.
     */
    static BatchLoadResult loadDigitalIntervalBinaryStateBatch(std::string const & filepath,
                                                               nlohmann::json const & config);

    /**
     * @brief Save LineData to CSV
     */
    static LoadResult saveLineDataCSV(std::string const & filepath,
                                      nlohmann::json const & config,
                                      void const * data);

    /**
     * @brief Save PointData to CSV
     */
    static LoadResult savePointDataCSV(std::string const & filepath,
                                       nlohmann::json const & config,
                                       void const * data);

    /**
     * @brief Save AnalogTimeSeries to CSV
     */
    static LoadResult saveAnalogCSV(std::string const & filepath,
                                    nlohmann::json const & config,
                                    void const * data);

    /**
     * @brief Save DigitalEventSeries to CSV
     */
    static LoadResult saveDigitalEventCSV(std::string const & filepath,
                                          nlohmann::json const & config,
                                          void const * data);

    /**
     * @brief Save DigitalIntervalSeries to CSV
     */
    static LoadResult saveDigitalIntervalCSV(std::string const & filepath,
                                             nlohmann::json const & config,
                                             void const * data);

    /**
     * @brief Load MaskData from CSV with RLE encoding
     */
    static LoadResult loadMaskDataCSV(std::string const & filepath,
                                      nlohmann::json const & config);

    /**
     * @brief Save MaskData to CSV with RLE encoding
     */
    static LoadResult saveMaskDataCSV(std::string const & filepath,
                                      nlohmann::json const & config,
                                      void const * data);

    /**
     * @brief Load TensorData from CSV
     */
    static LoadResult loadTensorDataCSV(std::string const & filepath,
                                        nlohmann::json const & config);

    /**
     * @brief Save TensorData to CSV
     */
    static LoadResult saveTensorDataCSV(std::string const & filepath,
                                        nlohmann::json const & config,
                                        void const * data);
};

#endif// CSV_FORMAT_LOADER_HPP
