/**
 * @file TimeIndexExtractor.cpp
 * @brief Implementation of extractTimeIndices() — a type-erased time-index
 *        extractor for any DataManager data source.
 *
 * This file includes every concrete data type header so that the rest of the
 * codebase (in particular, UI widgets like TensorDesigner) does not have to.
 */

#include "TimeIndexExtractor.hpp"

#include "DataManager/DataManager.hpp"
#include "AnalogTimeSeries/Analog_Time_Series.hpp"
#include "DataManager/DigitalTimeSeries/Digital_Event_Series.hpp"
#include "DataManager/DigitalTimeSeries/Digital_Interval_Series.hpp"
#include "DataManager/Lines/Line_Data.hpp"
#include "DataManager/Masks/Mask_Data.hpp"
#include "DataManager/Points/Point_Data.hpp"

TimeIndexResult extractTimeIndices(
        DataManager & dm,
        std::string const & key) {

    auto const source_type = dm.getType(key);

    switch (source_type) {

        case DM_DataType::Analog: {
            auto analog = dm.getData<AnalogTimeSeries>(key);
            if (!analog || analog->getNumSamples() == 0) {
                return {};
            }
            return {analog->getTimeSeries(), analog->getTimeFrame()};
        }

        case DM_DataType::DigitalEvent: {
            auto events = dm.getData<DigitalEventSeries>(key);
            if (!events || events->size() == 0) {
                return {};
            }
            std::vector<TimeFrameIndex> times;
            times.reserve(events->size());
            for (auto const & ew : events->view()) {
                times.push_back(ew.event_time);
            }
            return {std::move(times), events->getTimeFrame()};
        }

        case DM_DataType::DigitalInterval: {
            auto dis = dm.getData<DigitalIntervalSeries>(key);
            if (!dis || dis->size() == 0) {
                return {};
            }
            std::vector<TimeFrameIndex> times;
            times.reserve(dis->size());
            for (auto const & iw : dis->view()) {
                times.push_back(TimeFrameIndex(iw.interval.start));
            }
            return {std::move(times), dis->getTimeFrame()};
        }

        case DM_DataType::Mask: {
            auto masks = dm.getData<MaskData>(key);
            if (!masks) {
                return {};
            }
            std::vector<TimeFrameIndex> times;
            for (auto t : masks->getTimesWithData()) {
                times.push_back(t);
            }
            return {std::move(times), masks->getTimeFrame()};
        }

        case DM_DataType::Line: {
            auto lines = dm.getData<LineData>(key);
            if (!lines) {
                return {};
            }
            std::vector<TimeFrameIndex> times;
            for (auto t : lines->getTimesWithData()) {
                times.push_back(t);
            }
            return {std::move(times), lines->getTimeFrame()};
        }

        case DM_DataType::Points: {
            auto points = dm.getData<PointData>(key);
            if (!points) {
                return {};
            }
            std::vector<TimeFrameIndex> times;
            for (auto t : points->getTimesWithData()) {
                times.push_back(t);
            }
            return {std::move(times), points->getTimeFrame()};
        }

        // Types without meaningful time indices
        case DM_DataType::Video:
        case DM_DataType::Images:
        case DM_DataType::RaggedAnalog:
        case DM_DataType::Tensor:
        case DM_DataType::Time:
        case DM_DataType::Unknown:
            return {};
    }

    return {}; // unreachable, but silences warnings
}
