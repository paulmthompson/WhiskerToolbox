/// @file DataManagerKeys.cpp
/// @brief Implementation of DataManager key-query utilities.

#include "DataManagerKeys.hpp"

#include "DataManager/DataManager.hpp"

#include "AnalogTimeSeries/Analog_Time_Series.hpp"
#include "AnalogTimeSeries/RaggedAnalogTimeSeries.hpp"
#include "DigitalTimeSeries/Digital_Event_Series.hpp"
#include "DigitalTimeSeries/Digital_Interval_Series.hpp"
#include "Lines/Line_Data.hpp"
#include "Masks/Mask_Data.hpp"
#include "Media/Media_Data.hpp"
#include "Points/Point_Data.hpp"
#include "Tensors/TensorData.hpp"

#include <algorithm>

std::vector<std::string>
getKeysForTypes(DataManager & dm, std::vector<DM_DataType> const & types) {
    if (types.empty()) {
        return dm.getAllKeys();
    }

    std::vector<std::string> result;
    for (auto const type: types) {
        std::vector<std::string> keys;
        switch (type) {
            case DM_DataType::Video:
            case DM_DataType::Images:
                keys = dm.getKeys<MediaData>();
                break;
            case DM_DataType::Points:
                keys = dm.getKeys<PointData>();
                break;
            case DM_DataType::Mask:
                keys = dm.getKeys<MaskData>();
                break;
            case DM_DataType::Line:
                keys = dm.getKeys<LineData>();
                break;
            case DM_DataType::Analog:
                keys = dm.getKeys<AnalogTimeSeries>();
                break;
            case DM_DataType::RaggedAnalog:
                keys = dm.getKeys<RaggedAnalogTimeSeries>();
                break;
            case DM_DataType::DigitalEvent:
                keys = dm.getKeys<DigitalEventSeries>();
                break;
            case DM_DataType::DigitalInterval:
                keys = dm.getKeys<DigitalIntervalSeries>();
                break;
            case DM_DataType::Tensor:
                keys = dm.getKeys<TensorData>();
                break;
            case DM_DataType::Time:
            case DM_DataType::Unknown:
                break;
        }
        result.insert(result.end(), keys.begin(), keys.end());
    }

    // Deduplicate (e.g. Video + Images both yield MediaData keys)
    std::sort(result.begin(), result.end());
    result.erase(std::unique(result.begin(), result.end()), result.end());

    return result;
}
