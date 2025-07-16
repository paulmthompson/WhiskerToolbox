#include "AnalogDataAdapter.h"

#include <stdexcept>

AnalogDataAdapter::AnalogDataAdapter(std::shared_ptr<AnalogTimeSeries> analogData, 
                                     std::shared_ptr<TimeFrame> timeFrame,
                                     std::string name)
    : m_analogData(std::move(analogData))
    , m_timeFrame(std::move(timeFrame))
    , m_name(std::move(name))
{
    if (!m_analogData) {
        throw std::invalid_argument("AnalogTimeSeries cannot be null");
    }
}

const std::string& AnalogDataAdapter::getName() const {
    return m_name;
}

std::shared_ptr<TimeFrame> AnalogDataAdapter::getTimeFrame() const {
    return m_timeFrame;
}

size_t AnalogDataAdapter::size() const {
    return m_analogData->getNumSamples();
}

std::span<const double> AnalogDataAdapter::getDataSpan() {
    if (!m_isMaterialized) {
        materializeData();
    }
    return std::span<const double>(m_materializedData);
}

std::vector<float> AnalogDataAdapter::getDataInRange(TimeFrameIndex start,
                                       TimeFrameIndex end,
                                       TimeFrame const * target_timeFrame) 
{
    auto const * timeFrame = m_timeFrame.get();
    auto data_span = m_analogData->getDataInTimeFrameIndexRange(start, end, target_timeFrame, timeFrame);
    return std::vector<float>(data_span.begin(), data_span.end());
}

void AnalogDataAdapter::materializeData() {
    if (m_isMaterialized) {
        return;
    }

    // Get the float data from AnalogTimeSeries
    const auto& floatData = m_analogData->getAnalogTimeSeries();
    
    // Convert to double
    m_materializedData.clear();
    m_materializedData.reserve(floatData.size());
    
    for (float value : floatData) {
        m_materializedData.push_back(static_cast<double>(value));
    }

    m_isMaterialized = true;
}
