#ifndef DATAMANAGER_IO_DATAFACTORY_HPP
#define DATAMANAGER_IO_DATAFACTORY_HPP

#include "DataLoader.hpp"
#include "CoreGeometry/points.hpp"
#include "CoreGeometry/lines.hpp"
#include "CoreGeometry/masks.hpp"
#include "TimeFrame/TimeFrame.hpp"

#include <memory>
#include <map>
#include <vector>

/**
 * @brief Raw data container for deserialized CapnProto data
 * 
 * This struct contains the raw data extracted from CapnProto format
 * without depending on specific data type implementations.
 */
struct LineDataRaw {
    std::map<int32_t, std::vector<Line2D>> time_lines;
    uint32_t image_width = 0;
    uint32_t image_height = 0;
};

/**
 * @brief Raw data container for deserialized Mask data
 * 
 * This struct contains the raw data extracted from HDF5/other formats
 * without depending on specific data type implementations.
 */
struct MaskDataRaw {
    std::map<int32_t, std::vector<Mask2D>> time_masks;
    uint32_t image_width = 0;
    uint32_t image_height = 0;
};


/**
 * @brief Abstract factory interface for creating data objects
 * 
 * This allows plugins to create data objects without directly depending
 * on the concrete data type implementations.
 */
class DataFactory {
public:
    virtual ~DataFactory() = default;
    
    // LineData factory methods
    virtual LoadedDataVariant createLineData() = 0;
    virtual LoadedDataVariant createLineData(std::map<TimeFrameIndex, std::vector<Line2D>> const& data) = 0;
    virtual LoadedDataVariant createLineDataFromRaw(LineDataRaw const& raw_data) = 0;
    virtual void setLineDataImageSize(LoadedDataVariant& data, int width, int height) = 0;
    
    // MaskData factory methods
    virtual LoadedDataVariant createMaskData() = 0;
    virtual LoadedDataVariant createMaskDataFromRaw(MaskDataRaw const& raw_data) = 0;
    virtual void setMaskDataImageSize(LoadedDataVariant& data, int width, int height) = 0;
    
    // Future: PointData factory methods
    // virtual LoadedDataVariant createPointData() = 0;
};


#endif // DATAMANAGER_IO_DATAFACTORY_HPP
