#include "Lineage/DataManagerEntityDataSource.hpp"
#include "DataManager.hpp"

#include "AnalogTimeSeries/RaggedAnalogTimeSeries.hpp"
#include "DigitalTimeSeries/Digital_Event_Series.hpp"
#include "DigitalTimeSeries/Digital_Interval_Series.hpp"
#include "Lines/Line_Data.hpp"
#include "Masks/Mask_Data.hpp"
#include "Points/Point_Data.hpp"

namespace WhiskerToolbox::Lineage {

namespace {

// Helper to get EntityId at a specific local_index for RaggedTimeSeries types
template<typename T>
std::optional<EntityId> getEntityIdAtLocalIndex(
        std::shared_ptr<T> const & data,
        TimeFrameIndex time,
        std::size_t local_index) {
    auto ids_range = data->getEntityIdsAtTime(time);
    std::size_t idx = 0;
    for (auto const id : ids_range) {
        if (idx == local_index) {
            return id;
        }
        ++idx;
    }
    return std::nullopt;
}

// Helper to collect all EntityIds at a time for RaggedTimeSeries
template<typename T>
std::vector<EntityId> collectEntityIdsAtTime(
        std::shared_ptr<T> const & data,
        TimeFrameIndex time) {
    std::vector<EntityId> result;
    for (auto const id : data->getEntityIdsAtTime(time)) {
        result.push_back(id);
    }
    return result;
}

// Helper to extract all EntityIds from a RaggedTimeSeries using flattened_data()
template<typename T>
std::unordered_set<EntityId> extractEntityIdsFromRagged(std::shared_ptr<T> const & data) {
    std::unordered_set<EntityId> result;
    for (auto const & [time, entity_id, data_ref] : data->flattened_data()) {
        result.insert(entity_id);
    }
    return result;
}

// Helper to extract all EntityIds from types with getEntityIds() method
template<typename T>
std::unordered_set<EntityId> extractEntityIdsFromVector(std::shared_ptr<T> const & data) {
    auto const & ids = data->view();
    std::unordered_set<EntityId> result;
    for (auto const & item : ids) {
        result.insert(item.id());
    }
    return result;
}

// Helper to count elements at a specific time for RaggedTimeSeries
template<typename T>
std::size_t countElementsAtTime(std::shared_ptr<T> const & data, TimeFrameIndex time) {
    std::size_t count = 0;
    for ([[maybe_unused]] auto const id : data->getEntityIdsAtTime(time)) {
        ++count;
    }
    return count;
}

}// anonymous namespace

DataManagerEntityDataSource::DataManagerEntityDataSource(DataManager * dm)
    : _dm(dm) {
}

std::vector<EntityId> DataManagerEntityDataSource::getEntityIds(
        std::string const & data_key,
        TimeFrameIndex time,
        std::size_t local_index) const {
    if (!_dm) {
        return {};
    }

    DM_DataType const type = _dm->getType(data_key);

    switch (type) {
        case DM_DataType::Line:
            if (auto data = _dm->getData<LineData>(data_key)) {
                if (auto id = getEntityIdAtLocalIndex(data, time, local_index)) {
                    return {*id};
                }
            }
            break;

        case DM_DataType::Mask:
            if (auto data = _dm->getData<MaskData>(data_key)) {
                if (auto id = getEntityIdAtLocalIndex(data, time, local_index)) {
                    return {*id};
                }
            }
            break;

        case DM_DataType::Points:
            if (auto data = _dm->getData<PointData>(data_key)) {
                if (auto id = getEntityIdAtLocalIndex(data, time, local_index)) {
                    return {*id};
                }
            }
            break;

        case DM_DataType::DigitalEvent:
            if (auto data = _dm->getData<DigitalEventSeries>(data_key)) {
                auto const & events = data->view();
                for (auto const & event : events) {
                    if (event.time() == time) {
                        return {event.id()};
                    }
                }
            }
            break;

        case DM_DataType::DigitalInterval:
            if (auto data = _dm->getData<DigitalIntervalSeries>(data_key)) {
                auto const & intervals = data->view();
                for (auto const & interval : intervals) {
                    if (TimeFrameIndex(interval.value().start) <= time &&
                        time <= TimeFrameIndex(interval.value().end)) {
                        return {interval.id()};
                    }
                }
            }
            break;

        case DM_DataType::Video:
        case DM_DataType::Images:
        case DM_DataType::Analog:
        case DM_DataType::RaggedAnalog:
        case DM_DataType::Tensor:
        case DM_DataType::Time:
        case DM_DataType::Unknown:
            break;
    }

    return {};
}

std::vector<EntityId> DataManagerEntityDataSource::getAllEntityIdsAtTime(
        std::string const & data_key,
        TimeFrameIndex time) const {
    if (!_dm) {
        return {};
    }

    DM_DataType const type = _dm->getType(data_key);

    switch (type) {
        case DM_DataType::Line:
            if (auto data = _dm->getData<LineData>(data_key)) {
                return collectEntityIdsAtTime(data, time);
            }
            break;

        case DM_DataType::Mask:
            if (auto data = _dm->getData<MaskData>(data_key)) {
                return collectEntityIdsAtTime(data, time);
            }
            break;

        case DM_DataType::Points:
            if (auto data = _dm->getData<PointData>(data_key)) {
                return collectEntityIdsAtTime(data, time);
            }
            break;

        case DM_DataType::DigitalEvent:
            if (auto data = _dm->getData<DigitalEventSeries>(data_key)) {
                std::vector<EntityId> result;
                auto const & events = data->view();
                for (auto const & event : events) {
                    if (event.time() == time) {
                        result.push_back(event.id());
                    }
                }
                return result;
            }
            break;

        case DM_DataType::DigitalInterval:
            if (auto data = _dm->getData<DigitalIntervalSeries>(data_key)) {
                std::vector<EntityId> result;
                auto const & intervals = data->view();
                for (auto const & interval : intervals) {
                    if (TimeFrameIndex(interval.value().start) <= time &&
                        time <= TimeFrameIndex(interval.value().end)) {
                        result.push_back(interval.id());
                    }
                }
                return result;
            }
            break;

        case DM_DataType::Video:
        case DM_DataType::Images:
        case DM_DataType::Analog:
        case DM_DataType::RaggedAnalog:
        case DM_DataType::Tensor:
        case DM_DataType::Time:
        case DM_DataType::Unknown:
            break;
    }

    return {};
}

std::unordered_set<EntityId> DataManagerEntityDataSource::getAllEntityIds(
        std::string const & data_key) const {
    if (!_dm) {
        return {};
    }

    DM_DataType const type = _dm->getType(data_key);

    switch (type) {
        case DM_DataType::Line:
            if (auto data = _dm->getData<LineData>(data_key)) {
                return extractEntityIdsFromRagged(data);
            }
            break;

        case DM_DataType::Mask:
            if (auto data = _dm->getData<MaskData>(data_key)) {
                return extractEntityIdsFromRagged(data);
            }
            break;

        case DM_DataType::Points:
            if (auto data = _dm->getData<PointData>(data_key)) {
                return extractEntityIdsFromRagged(data);
            }
            break;

        case DM_DataType::DigitalEvent:
            if (auto data = _dm->getData<DigitalEventSeries>(data_key)) {
                return extractEntityIdsFromVector(data);
            }
            break;

        case DM_DataType::DigitalInterval:
            if (auto data = _dm->getData<DigitalIntervalSeries>(data_key)) {
                return extractEntityIdsFromVector(data);
            }
            break;

        // Types without EntityIds
        case DM_DataType::Video:
        case DM_DataType::Images:
        case DM_DataType::Analog:
        case DM_DataType::RaggedAnalog:
        case DM_DataType::Tensor:
        case DM_DataType::Time:
        case DM_DataType::Unknown:
            break;
    }

    return {};
}

std::size_t DataManagerEntityDataSource::getElementCount(
        std::string const & data_key,
        TimeFrameIndex time) const {
    if (!_dm) {
        return 0;
    }

    DM_DataType const type = _dm->getType(data_key);

    switch (type) {
        case DM_DataType::RaggedAnalog:
            if (auto data = _dm->getData<RaggedAnalogTimeSeries>(data_key)) {
                auto values = data->getDataAtTime(time);
                return values.size();
            }
            break;

        case DM_DataType::Mask:
            if (auto data = _dm->getData<MaskData>(data_key)) {
                return countElementsAtTime(data, time);
            }
            break;

        case DM_DataType::Line:
            if (auto data = _dm->getData<LineData>(data_key)) {
                return countElementsAtTime(data, time);
            }
            break;

        case DM_DataType::Points:
            if (auto data = _dm->getData<PointData>(data_key)) {
                return countElementsAtTime(data, time);
            }
            break;

        case DM_DataType::DigitalEvent:
            if (auto data = _dm->getData<DigitalEventSeries>(data_key)) {
                std::size_t count = 0;
                for (auto const & event : data->view()) {
                    if (event.time() == time) {
                        ++count;
                    }
                }
                return count;
            }
            break;

        case DM_DataType::DigitalInterval:
            if (auto data = _dm->getData<DigitalIntervalSeries>(data_key)) {
                std::size_t count = 0;
                for (auto const & interval : data->view()) {
                    if (TimeFrameIndex(interval.value().start) <= time && time <= TimeFrameIndex(interval.value().end)) {
                        ++count;
                    }
                }
                return count;
            }
            break;

        // Single-element types
        case DM_DataType::Video:
        case DM_DataType::Images:
        case DM_DataType::Analog:
        case DM_DataType::Tensor:
        case DM_DataType::Time:
            return 1;

        case DM_DataType::Unknown:
            break;
    }

    return 0;
}

}// namespace WhiskerToolbox::Lineage
