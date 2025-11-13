#include "AnalogDataAdapter.h"

#include "AnalogTimeSeries/Analog_Time_Series.hpp"

#include <stdexcept>

AnalogDataAdapter::AnalogDataAdapter(std::shared_ptr<AnalogTimeSeries> analogData,
                                     std::shared_ptr<TimeFrame> timeFrame,
                                     std::string name)
    : m_analogData(std::move(analogData)),
      m_timeFrame(std::move(timeFrame)),
      m_name(std::move(name)) {
    if (!m_analogData) {
        throw std::invalid_argument("AnalogTimeSeries cannot be null");
    }
}

std::string const & AnalogDataAdapter::getName() const {
    return m_name;
}

std::shared_ptr<TimeFrame> AnalogDataAdapter::getTimeFrame() const {
    return m_timeFrame;
}

size_t AnalogDataAdapter::size() const {
    return m_analogData->getNumSamples();
}

std::vector<float> AnalogDataAdapter::getDataInRange(TimeFrameIndex start,
                                                     TimeFrameIndex end,
                                                     TimeFrame const * target_timeFrame) {

    auto data_span = m_analogData->getDataInTimeFrameIndexRange(start, end, target_timeFrame);
    return std::vector<float>(data_span.begin(), data_span.end());
}
