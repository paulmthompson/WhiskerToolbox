#include "RaggedAnalogTimeSeries.hpp"

#include <algorithm>
#include <stdexcept>

// ========== Time Frame ==========

void RaggedAnalogTimeSeries::setTimeFrame(std::shared_ptr<TimeFrame> time_frame) {
    _time_frame = std::move(time_frame);
}

std::shared_ptr<TimeFrame> RaggedAnalogTimeSeries::getTimeFrame() const {
    return _time_frame;
}

// ========== Data Access ==========

std::span<float const> RaggedAnalogTimeSeries::getDataAtTime(TimeFrameIndex time) const {
    return _storage.getValuesAtTime(time);
}

std::span<float const> RaggedAnalogTimeSeries::getDataAtTime(TimeIndexAndFrame const & time_index_and_frame) const {
    TimeFrameIndex converted_time = _convertTimeIndex(time_index_and_frame);
    return getDataAtTime(converted_time);
}

std::vector<float> RaggedAnalogTimeSeries::getValuesAtTimeVec(TimeFrameIndex time) const {
    auto [start, end] = _storage.getTimeRange(time);
    if (start >= end) {
        return {};
    }
    
    std::vector<float> result;
    result.reserve(end - start);
    for (size_t i = start; i < end; ++i) {
        result.push_back(_storage.getValue(i));
    }
    return result;
}

bool RaggedAnalogTimeSeries::hasDataAtTime(TimeFrameIndex time) const {
    return _storage.hasDataAtTime(time);
}

size_t RaggedAnalogTimeSeries::getCountAtTime(TimeFrameIndex time) const {
    auto [start, end] = _storage.getTimeRange(time);
    return end - start;
}

std::vector<TimeFrameIndex> RaggedAnalogTimeSeries::getTimeIndices() const {
    std::vector<TimeFrameIndex> indices;
    auto const& time_ranges = _storage.timeRanges();
    indices.reserve(time_ranges.size());
    
    for (auto const & [time, range] : time_ranges) {
        (void)range;  // unused
        indices.push_back(time);
    }
    
    return indices;
}

size_t RaggedAnalogTimeSeries::getNumTimePoints() const {
    return _storage.getTimeCount();
}

// ========== Data Modification ==========

void RaggedAnalogTimeSeries::setDataAtTime(TimeFrameIndex time, std::vector<float> const & data, NotifyObservers notify) {
    _invalidateStorageCache();
    _storage.setAtTime(time, data);
    _updateStorageCache();
    
    if (notify == NotifyObservers::Yes) {
        notifyObservers();
    }
}

void RaggedAnalogTimeSeries::setDataAtTime(TimeFrameIndex time, std::vector<float> && data, NotifyObservers notify) {
    _invalidateStorageCache();
    // setAtTime copies anyway since it may need to remove existing data first
    _storage.setAtTime(time, data);
    _updateStorageCache();
    
    if (notify == NotifyObservers::Yes) {
        notifyObservers();
    }
}

void RaggedAnalogTimeSeries::setDataAtTime(TimeIndexAndFrame const & time_index_and_frame, std::vector<float> const & data, NotifyObservers notify) {
    TimeFrameIndex converted_time = _convertTimeIndex(time_index_and_frame);
    setDataAtTime(converted_time, data, notify);
}

void RaggedAnalogTimeSeries::setDataAtTime(TimeIndexAndFrame const & time_index_and_frame, std::vector<float> && data, NotifyObservers notify) {
    TimeFrameIndex converted_time = _convertTimeIndex(time_index_and_frame);
    setDataAtTime(converted_time, std::move(data), notify);
}

void RaggedAnalogTimeSeries::appendAtTime(TimeFrameIndex time, std::vector<float> const & data, NotifyObservers notify) {
    _invalidateStorageCache();
    _storage.appendBatch(time, data);
    _updateStorageCache();
    
    if (notify == NotifyObservers::Yes) {
        notifyObservers();
    }
}

void RaggedAnalogTimeSeries::appendAtTime(TimeFrameIndex time, std::vector<float> && data, NotifyObservers notify) {
    _invalidateStorageCache();
    _storage.appendBatch(time, std::move(data));
    _updateStorageCache();
    
    if (notify == NotifyObservers::Yes) {
        notifyObservers();
    }
}

bool RaggedAnalogTimeSeries::clearAtTime(TimeFrameIndex time, NotifyObservers notify) {
    if (!_storage.hasDataAtTime(time)) {
        return false;
    }
    
    _invalidateStorageCache();
    _storage.removeAtTime(time);
    _updateStorageCache();
    
    if (notify == NotifyObservers::Yes) {
        notifyObservers();
    }
    
    return true;
}

bool RaggedAnalogTimeSeries::clearAtTime(TimeIndexAndFrame const & time_index_and_frame, NotifyObservers notify) {
    TimeFrameIndex converted_time = _convertTimeIndex(time_index_and_frame);
    return clearAtTime(converted_time, notify);
}

void RaggedAnalogTimeSeries::clearAll(NotifyObservers notify) {
    _invalidateStorageCache();
    _storage.clear();
    _updateStorageCache();
    
    if (notify == NotifyObservers::Yes) {
        notifyObservers();
    }
}

// ========== Private Methods ==========

TimeFrameIndex RaggedAnalogTimeSeries::_convertTimeIndex(TimeIndexAndFrame const & time_index_and_frame) const {
    // If the source timeframe is the same as ours, no conversion needed
    if (time_index_and_frame.time_frame == _time_frame.get()) {
        return time_index_and_frame.index;
    }
    
    // If either timeframe is null, just use the index directly
    if (!time_index_and_frame.time_frame || !_time_frame) {
        return time_index_and_frame.index;
    }
    
    // Convert the time index from source timeframe to target timeframe
    // 1. Get the time value from the source timeframe
    auto time_value = time_index_and_frame.time_frame->getTimeAtIndex(time_index_and_frame.index);
    
    // 2. Convert that time value to an index in our timeframe
    return _time_frame->getIndexAtTime(static_cast<float>(time_value), false);
}
