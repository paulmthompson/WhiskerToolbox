#include "ViewFactory.hpp"

#include "BaseDataView.hpp"

// Include all type-specific views
#include "DataInspector_Widget/PointData/PointTableView.hpp"
#include "DataInspector_Widget/LineData/LineTableView.hpp"
#include "DataInspector_Widget/MaskData/MaskTableView.hpp"
#include "DataInspector_Widget/ImageData/ImageDataView.hpp"
#include "DataInspector_Widget/AnalogTimeSeries/AnalogTimeSeriesDataView.hpp"
#include "DataInspector_Widget/DigitalEventSeries/DigitalEventSeriesDataView.hpp"
#include "DataInspector_Widget/DigitalIntervalSeries/DigitalIntervalSeriesDataView.hpp"
#include "DataInspector_Widget/TensorData/TensorDataView.hpp"

std::unique_ptr<BaseDataView> ViewFactory::createView(
    DM_DataType type,
    std::shared_ptr<DataManager> data_manager,
    QWidget * parent) {
    
    switch (type) {
        case DM_DataType::Points:
            return std::make_unique<PointTableView>(data_manager, parent);
        
        case DM_DataType::Line:
            return std::make_unique<LineTableView>(data_manager, parent);
        
        case DM_DataType::Mask:
            return std::make_unique<MaskTableView>(data_manager, parent);
        
        case DM_DataType::Images:
        case DM_DataType::Video:
            return std::make_unique<ImageDataView>(data_manager, parent);
        
        case DM_DataType::Analog:
            return std::make_unique<AnalogTimeSeriesDataView>(data_manager, parent);
        
        case DM_DataType::DigitalEvent:
            return std::make_unique<DigitalEventSeriesDataView>(data_manager, parent);
        
        case DM_DataType::DigitalInterval:
            return std::make_unique<DigitalIntervalSeriesDataView>(data_manager, parent);
        
        case DM_DataType::Tensor:
            return std::make_unique<TensorDataView>(data_manager, parent);
        
        case DM_DataType::Unknown:
        case DM_DataType::RaggedAnalog:
        case DM_DataType::Time:
        default:
            return nullptr;
    }
}

bool ViewFactory::hasView(DM_DataType type) {
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
        
        case DM_DataType::Unknown:
        case DM_DataType::RaggedAnalog:
        case DM_DataType::Time:
        default:
            return false;
    }
}
