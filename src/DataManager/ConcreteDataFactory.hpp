#ifndef DATAMANAGER_CONCRETEDATAFACTORY_HPP
#define DATAMANAGER_CONCRETEDATAFACTORY_HPP

#include "IO/interface/DataFactory.hpp"
#include "IO/interface/DataLoader.hpp"

#include "TimeFrame/TimeFrame.hpp"
#include "CoreGeometry/lines.hpp"
#include "CoreGeometry/points.hpp"

/**
 * @brief Concrete factory implementation that creates actual data objects
 * 
 * This implementation is provided by DataManager and injected into plugins.
 * It has access to all the concrete data type implementations.
 */
class ConcreteDataFactory : public DataFactory {
public:
    LoadedDataVariant createLineData() override;
    LoadedDataVariant createLineData(std::map<TimeFrameIndex, std::vector<Line2D>> const& data) override;
    LoadedDataVariant createLineDataFromRaw(LineDataRaw const& raw_data) override;
    void setLineDataImageSize(LoadedDataVariant& data, int width, int height) override;
    
    // MaskData factory methods
    LoadedDataVariant createMaskData() override;
    LoadedDataVariant createMaskDataFromRaw(MaskDataRaw const& raw_data) override;
    void setMaskDataImageSize(LoadedDataVariant& data, int width, int height) override;
    
    // Future implementations for other data types...
};

/**
 * @brief Helper function to convert raw line data to proper types
 */
std::map<TimeFrameIndex, std::vector<Line2D>> convertRawLineData(LineDataRaw const& raw_data);

/**
 * @brief Helper function to convert raw mask data to proper types
 */
std::map<TimeFrameIndex, std::vector<std::vector<Point2D<uint32_t>>>> convertRawMaskData(MaskDataRaw const& raw_data);

#endif // DATAMANAGER_CONCRETEDATAFACTORY_HPP
