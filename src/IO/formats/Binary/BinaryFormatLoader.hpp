#ifndef BINARY_FORMAT_LOADER_HPP
#define BINARY_FORMAT_LOADER_HPP

#include "../../core/LoaderRegistry.hpp"

/**
 * @brief Format-centric binary loader for all binary file data types
 * 
 * This loader follows the format-centric architecture where one loader
 * handles a specific file format (binary) for all applicable data types.
 * 
 * Supported data types:
 * - DM_DataType::Analog: Multi-channel analog time series (int16, float32, etc.)
 * - DM_DataType::DigitalEvent: Digital events extracted from TTL channels
 * - DM_DataType::DigitalInterval: Digital intervals extracted from TTL channels
 * 
 * This loader supports batch loading for multi-channel binary files,
 * returning all channels in a single loadBatch() call.
 * 
 * Configuration options vary by data type - see individual load methods
 * for required and optional JSON configuration fields.
 */
class BinaryFormatLoader : public IFormatLoader {
public:
    BinaryFormatLoader() = default;
    ~BinaryFormatLoader() override = default;

    /**
     * @brief Load a single data object from binary file
     * 
     * For multi-channel binary files (Analog with num_channels > 1),
     * this returns only the first channel. Use loadBatch() for all channels.
     */
    LoadResult load(std::string const & filepath,
                    DM_DataType dataType,
                    nlohmann::json const & config) const override;

    /**
     * @brief Check if batch loading is supported for this format/type
     * 
     * Returns true for:
     * - Analog binary with num_channels > 1
     * - DigitalEvent (potentially multiple TTL channels)
     * - DigitalInterval (potentially multiple TTL channels)
     */
    bool supportsBatchLoading(std::string const & format,
                              DM_DataType dataType) const override;

    /**
     * @brief Load all data objects from a multi-channel binary file
     * 
     * For Analog data type, returns one AnalogTimeSeries per channel.
     * For Digital types, currently returns single object (future: multiple TTL channels).
     */
    BatchLoadResult loadBatch(std::string const & filepath,
                              DM_DataType dataType,
                              nlohmann::json const & config) const override;

    /**
     * @brief Save is not supported for binary format
     */
    LoadResult save(std::string const & filepath,
                    DM_DataType dataType,
                    nlohmann::json const & config,
                    void const * data) const override;

    /**
     * @brief Check if this loader supports the format/dataType combination
     * 
     * Supports format "binary" for Analog, DigitalEvent, DigitalInterval
     * Supports format "uint16" for DigitalEvent, DigitalInterval (legacy compatibility)
     */
    bool supportsFormat(std::string const & format, DM_DataType dataType) const override;

    /**
     * @brief Get loader name for logging
     */
    std::string getLoaderName() const override;

private:
    /**
     * @brief Load all channels from binary file as AnalogTimeSeries
     * @return BatchLoadResult with one LoadResult per channel
     */
    static BatchLoadResult loadAnalogBatch(std::string const & filepath,
                                    nlohmann::json const & config) ;

    /**
     * @brief Load DigitalEventSeries from uint16 binary file with TTL extraction
     */
    static LoadResult loadDigitalEvent(std::string const & filepath,
                                nlohmann::json const & config) ;

    /**
     * @brief Load DigitalIntervalSeries from uint16 binary file with TTL extraction
     */
    static LoadResult loadDigitalInterval(std::string const & filepath,
                                   nlohmann::json const & config) ;
};

#endif// BINARY_FORMAT_LOADER_HPP
