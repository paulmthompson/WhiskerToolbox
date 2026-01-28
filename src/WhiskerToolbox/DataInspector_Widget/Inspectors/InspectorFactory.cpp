#include "InspectorFactory.hpp"

#include "AnalogTimeSeries/AnalogTimeSeriesInspector.hpp"
#include "DigitalEventSeries/DigitalEventSeriesInspector.hpp"
#include "DigitalIntervalSeries/DigitalIntervalSeriesInspector.hpp"
#include "ImageData/ImageInspector.hpp"
#include "LineData/LineInspector.hpp"
#include "MaskData/MaskInspector.hpp"
#include "PointData/PointInspector.hpp"
#include "TensorData/TensorInspector.hpp"

std::unique_ptr<BaseInspector> InspectorFactory::createInspector(
    DM_DataType type,
    std::shared_ptr<DataManager> data_manager,
    GroupManager * group_manager,
    QWidget * parent) {
    
    switch (type) {
        case DM_DataType::Points:
            return std::make_unique<PointInspector>(
                std::move(data_manager), group_manager, parent);

        case DM_DataType::Line:
            return std::make_unique<LineInspector>(
                std::move(data_manager), group_manager, parent);

        case DM_DataType::Mask:
            return std::make_unique<MaskInspector>(
                std::move(data_manager), group_manager, parent);

        case DM_DataType::Images:
        case DM_DataType::Video:
            return std::make_unique<ImageInspector>(
                std::move(data_manager), group_manager, parent);

        case DM_DataType::Analog:
            return std::make_unique<AnalogTimeSeriesInspector>(
                std::move(data_manager), group_manager, parent);

        case DM_DataType::DigitalEvent:
            return std::make_unique<DigitalEventSeriesInspector>(
                std::move(data_manager), group_manager, parent);

        case DM_DataType::DigitalInterval:
            return std::make_unique<DigitalIntervalSeriesInspector>(
                std::move(data_manager), group_manager, parent);

        case DM_DataType::Tensor:
            return std::make_unique<TensorInspector>(
                std::move(data_manager), group_manager, parent);

        default:
            return nullptr;
    }
}

bool InspectorFactory::hasInspector(DM_DataType type) {
    switch (type) {
        case DM_DataType::Points:
        case DM_DataType::Line:
        case DM_DataType::Mask:
        case DM_DataType::Images:
        case DM_DataType::Video:
        case DM_DataType::Analog:
        case DM_DataType::DigitalEvent:
        case DM_DataType::DigitalInterval:
        case DM_DataType::Tensor:
            return true;

        default:
            return false;
    }
}
