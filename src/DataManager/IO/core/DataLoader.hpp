#ifndef DATAMANAGER_IO_DATALOADER_HPP
#define DATAMANAGER_IO_DATALOADER_HPP

#include "IOTypes.hpp"

#include "nlohmann/json.hpp"

#include <memory>
#include <string>
#include <variant>

class LineData;
class PointData;
class MaskData;
class ImageData;
class AnalogTimeSeries;
class DigitalEventSeries;
class DigitalIntervalSeries;
class TensorData;

/**
 * @brief Variant type for all possible data types that can be loaded
 */
using LoadedDataVariant = std::variant<
    std::shared_ptr<LineData>,
    std::shared_ptr<PointData>,
    std::shared_ptr<MaskData>,
    std::shared_ptr<ImageData>,
    std::shared_ptr<AnalogTimeSeries>,
    std::shared_ptr<DigitalEventSeries>,
    std::shared_ptr<DigitalIntervalSeries>,
    std::shared_ptr<TensorData>
>;

/**
 * @brief Result of a data loading operation
 */
struct LoadResult {
    bool success = false;
    std::string error_message;
    LoadedDataVariant data;
    std::string name;  ///< Optional name for batch loading (e.g., channel name, bodypart name)
    
    LoadResult() = default;
    LoadResult(LoadedDataVariant&& loaded_data) 
        : success(true), data(std::move(loaded_data)) {}
    LoadResult(LoadedDataVariant&& loaded_data, std::string const& result_name) 
        : success(true), data(std::move(loaded_data)), name(result_name) {}
    LoadResult(std::string const& error) 
        : success(false), error_message(error) {}
};

/**
 * @brief Abstract base class for data loaders
 * 
 * Each format plugin (e.g., CapnProto, HDF5) should inherit from this
 * and implement loading for the data types it supports.
 */
class DataLoader {
public:
    virtual ~DataLoader() = default;
    
    /**
     * @brief Get the format identifier (e.g., "capnp", "hdf5", "binary")
     */
    virtual std::string getFormatId() const = 0;
    
    /**
     * @brief Check if this loader supports the given data type
     */
    virtual bool supportsDataType(IODataType data_type) const = 0;
    
    /**
     * @brief Load data from file
     * 
     * @param file_path Path to the data file
     * @param data_type The type of data to load
     * @param config JSON configuration from the loading config
     * @return LoadResult containing the loaded data or error information
     */
    virtual LoadResult loadData(
        std::string const& file_path,
        IODataType data_type,
        nlohmann::json const& config
    ) const = 0;
};

#endif // DATAMANAGER_IO_DATALOADER_HPP
