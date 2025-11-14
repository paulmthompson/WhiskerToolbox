#include "DataManagerExtension.h"

#include "DataManager.hpp"
#include "DigitalTimeSeries/Digital_Event_Series.hpp"
#include "DigitalTimeSeries/Digital_Interval_Series.hpp"
#include "Lines/Line_Data.hpp"
#include "Points/Point_Data.hpp"
#include "utils/TableView/adapters/AnalogDataAdapter.h"
#include "utils/TableView/interfaces/IAnalogSource.h"


#include <iostream>

DataManagerExtension::DataManagerExtension(DataManager & dataManager)
    : m_dataManager(dataManager) {
}

std::shared_ptr<IAnalogSource> DataManagerExtension::getAnalogSource(std::string const & name) {
    // Check cache first
    auto cacheIt = m_dataSourceCache.find(name);
    if (cacheIt != m_dataSourceCache.end()) {
        return cacheIt->second;
    }

    // Try as physical analog data
    auto source = createAnalogDataAdapter(name);

    // Cache the result (even if nullptr) to avoid repeated lookups
    m_dataSourceCache[name] = source;
    return source;
}

void DataManagerExtension::clearCache() {
    m_dataSourceCache.clear();
}

std::shared_ptr<IAnalogSource> DataManagerExtension::createAnalogDataAdapter(std::string const & name) {
    try {
        auto analogData = m_dataManager.getData<AnalogTimeSeries>(name);
        if (!analogData) {
            return nullptr;
        }

        auto timeFrame_key = m_dataManager.getTimeKey(name);
        auto timeFrame = m_dataManager.getTime(timeFrame_key);

        return std::make_shared<AnalogDataAdapter>(analogData, timeFrame, name);
    } catch (std::exception const & e) {
        std::cerr << "Error creating AnalogDataAdapter for '" << name << "': " << e.what() << std::endl;
        return nullptr;
    }
}

template<typename T>
std::shared_ptr<T> DataManagerExtension::createDataOfType(std::string const & name) {
    auto data = m_dataManager.getDataVariant(name);
    if (!data.has_value()) {
        return nullptr;
    }
    if (auto typedData = std::get_if<std::shared_ptr<T>>(&data.value())) {
        return *typedData;
    } else {
        return nullptr;
    }
}

std::shared_ptr<LineData> DataManagerExtension::getLineSource(std::string const & name) {
    return createDataOfType<LineData>(name);
}
std::shared_ptr<PointData> DataManagerExtension::getPointSource(std::string const & name) {
    return createDataOfType<PointData>(name);
}
std::shared_ptr<DigitalEventSeries> DataManagerExtension::getEventSource(std::string const & name) {
    return createDataOfType<DigitalEventSeries>(name);
}
std::shared_ptr<DigitalIntervalSeries> DataManagerExtension::getIntervalSource(std::string const & name) {
    return createDataOfType<DigitalIntervalSeries>(name);
}

std::optional<DataTypeVariant> DataManagerExtension::getDataVariant(std::string const & name) {
    return m_dataManager.getDataVariant(name);
}

std::optional<DataManagerExtension::SourceHandle> DataManagerExtension::resolveSource(std::string const & name) {
    // Try cached adapters in the typical order of usage
    if (auto a = getAnalogSource(name)) {
        return a;
    }
    if (auto i = createDataOfType<DigitalIntervalSeries>(name)) {
        return i;
    }
    if (auto e = createDataOfType<DigitalEventSeries>(name)) {
        return e;
    }
    if (auto l = createDataOfType<LineData>(name)) {
        return l;
    }
    if (auto p = createDataOfType<PointData>(name)) {
        return p;
    }
    return std::nullopt;
}