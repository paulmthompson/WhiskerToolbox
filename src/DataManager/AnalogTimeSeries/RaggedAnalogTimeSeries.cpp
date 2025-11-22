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
    auto it = _data.find(time);
    if (it == _data.end()) {
        return {};
    }
    return std::span<float const>(it->second);
}

std::span<float const> RaggedAnalogTimeSeries::getDataAtTime(TimeIndexAndFrame const & time_index_and_frame) const {
    TimeFrameIndex converted_time = _convertTimeIndex(time_index_and_frame);
    return getDataAtTime(converted_time);
}

bool RaggedAnalogTimeSeries::hasDataAtTime(TimeFrameIndex time) const {
    return _data.contains(time);
}

size_t RaggedAnalogTimeSeries::getCountAtTime(TimeFrameIndex time) const {
    auto it = _data.find(time);
    if (it == _data.end()) {
        return 0;
    }
    return it->second.size();
}

std::vector<TimeFrameIndex> RaggedAnalogTimeSeries::getTimeIndices() const {
    std::vector<TimeFrameIndex> indices;
    indices.reserve(_data.size());
    
    for (auto const & [time, values] : _data) {
        indices.push_back(time);
    }
    
    return indices;
}

size_t RaggedAnalogTimeSeries::getNumTimePoints() const {
    return _data.size();
}

// ========== Data Modification ==========

void RaggedAnalogTimeSeries::setDataAtTime(TimeFrameIndex time, std::vector<float> const & data, NotifyObservers notify) {
    _data[time] = data;
    
    if (notify == NotifyObservers::Yes) {
        notifyObservers();
    }
}

void RaggedAnalogTimeSeries::setDataAtTime(TimeFrameIndex time, std::vector<float> && data, NotifyObservers notify) {
    _data[time] = std::move(data);
    
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
    auto & existing = _data[time];
    existing.insert(existing.end(), data.begin(), data.end());
    
    if (notify == NotifyObservers::Yes) {
        notifyObservers();
    }
}

void RaggedAnalogTimeSeries::appendAtTime(TimeFrameIndex time, std::vector<float> && data, NotifyObservers notify) {
    auto & existing = _data[time];
    existing.insert(existing.end(), 
                   std::make_move_iterator(data.begin()), 
                   std::make_move_iterator(data.end()));
    
    if (notify == NotifyObservers::Yes) {
        notifyObservers();
    }
}

bool RaggedAnalogTimeSeries::clearAtTime(TimeFrameIndex time, NotifyObservers notify) {
    auto it = _data.find(time);
    if (it == _data.end()) {
        return false;
    }
    
    _data.erase(it);
    
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
    _data.clear();
    
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
