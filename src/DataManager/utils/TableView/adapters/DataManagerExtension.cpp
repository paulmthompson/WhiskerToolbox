#include "DataManagerExtension.h"

#include "DataManager.hpp"
#include "Points/Point_Data.hpp"
#include "utils/TableView/adapters/AnalogDataAdapter.h"
#include "utils/TableView/adapters/DigitalEventDataAdapter.h"
#include "utils/TableView/adapters/DigitalIntervalDataAdapter.h"
#include "utils/TableView/adapters/LineDataAdapter.h"
#include "utils/TableView/interfaces/IAnalogSource.h"
#include "utils/TableView/interfaces/IEventSource.h"
#include "utils/TableView/interfaces/IIntervalSource.h"
#include "utils/TableView/interfaces/ILineSource.h"


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
    m_eventSourceCache.clear();
    m_intervalSourceCache.clear();
    m_lineSourceCache.clear();
    m_pointDataCache.clear();
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

std::shared_ptr<IEventSource> DataManagerExtension::createDigitalEventDataAdapter(std::string const & name) {
    try {
        auto digitalEventSeries = m_dataManager.getData<DigitalEventSeries>(name);
        if (!digitalEventSeries) {
            return nullptr;
        }

        auto timeFrame_key = m_dataManager.getTimeKey(name);
        auto timeFrame = m_dataManager.getTime(timeFrame_key);

        return std::make_shared<DigitalEventDataAdapter>(digitalEventSeries, timeFrame, name);
    } catch (std::exception const & e) {
        std::cerr << "Error creating DigitalEventDataAdapter for '" << name << "': " << e.what() << std::endl;
        return nullptr;
    }
}

std::shared_ptr<IIntervalSource> DataManagerExtension::createDigitalIntervalDataAdapter(std::string const & name) {
    try {
        auto digitalIntervalSeries = m_dataManager.getData<DigitalIntervalSeries>(name);
        if (!digitalIntervalSeries) {
            return nullptr;
        }

        auto timeFrame_key = m_dataManager.getTimeKey(name);
        auto timeFrame = m_dataManager.getTime(timeFrame_key);

        return std::make_shared<DigitalIntervalDataAdapter>(digitalIntervalSeries, timeFrame, name);
    } catch (std::exception const & e) {
        std::cerr << "Error creating DigitalIntervalDataAdapter for '" << name << "': " << e.what() << std::endl;
        return nullptr;
    }
}

std::shared_ptr<ILineSource> DataManagerExtension::createLineDataAdapter(std::string const & name) {
    try {
        auto lineData = m_dataManager.getData<LineData>(name);
        if (!lineData) {
            return nullptr;
        }

        auto timeFrame_key = m_dataManager.getTimeKey(name);
        auto timeFrame = m_dataManager.getTime(timeFrame_key);

        return std::make_shared<LineDataAdapter>(lineData, timeFrame, name);
    } catch (std::exception const & e) {
        std::cerr << "Error creating LineDataAdapter for '" << name << "': " << e.what() << std::endl;
        return nullptr;
    }
}

std::shared_ptr<PointData> DataManagerExtension::createPointData(std::string const & name) {
    try {
        auto pointData = m_dataManager.getData<PointData>(name);
        if (!pointData) {
            return nullptr;
        }

        // PointData already has TimeFrame and EntityID information, no adapter needed
        return pointData;
    } catch (std::exception const & e) {
        std::cerr << "Error retrieving PointData for '" << name << "': " << e.what() << std::endl;
        return nullptr;
    }
}

std::shared_ptr<IEventSource> DataManagerExtension::getEventSource(std::string const & name) {
    // Check cache first
    auto it = m_eventSourceCache.find(name);
    if (it != m_eventSourceCache.end()) {
        return it->second;
    }

    // Create a new event source adapter
    auto eventSource = createDigitalEventDataAdapter(name);
    if (eventSource) {
        m_eventSourceCache[name] = eventSource;
    } else {
        std::cerr << "Event source '" << name << "' not found." << std::endl;
    }
    return eventSource;
}

std::shared_ptr<IIntervalSource> DataManagerExtension::getIntervalSource(std::string const & name) {
    // Check cache first
    auto it = m_intervalSourceCache.find(name);
    if (it != m_intervalSourceCache.end()) {
        return it->second;
    }

    // Create a new interval source adapter
    auto intervalSource = createDigitalIntervalDataAdapter(name);
    if (intervalSource) {
        m_intervalSourceCache[name] = intervalSource;
    } else {
        std::cerr << "Interval source '" << name << "' not found." << std::endl;
    }
    return intervalSource;
}

std::shared_ptr<ILineSource> DataManagerExtension::getLineSource(std::string const & name) {
    // Check cache first
    auto it = m_lineSourceCache.find(name);
    if (it != m_lineSourceCache.end()) {
        return it->second;
    }

    // Create a new line source adapter
    auto lineSource = createLineDataAdapter(name);
    if (lineSource) {
        m_lineSourceCache[name] = lineSource;
    } else {
        std::cerr << "Line source '" << name << "' not found." << std::endl;
    }
    return lineSource;
}

std::shared_ptr<PointData> DataManagerExtension::getPointData(std::string const & name) {
    // Check cache first
    auto it = m_pointDataCache.find(name);
    if (it != m_pointDataCache.end()) {
        return it->second;
    }

    // Create/retrieve point data
    auto pointData = createPointData(name);
    if (pointData) {
        m_pointDataCache[name] = pointData;
    } else {
        std::cerr << "Point data '" << name << "' not found." << std::endl;
    }
    return pointData;
}

std::optional<DataManagerExtension::SourceHandle> DataManagerExtension::resolveSource(std::string const & name) {
    // Try cached adapters in the typical order of usage
    if (auto a = getAnalogSource(name)) {
        return a;
    }
    if (auto i = getIntervalSource(name)) {
        return i;
    }
    if (auto e = getEventSource(name)) {
        return e;
    }
    if (auto l = getLineSource(name)) {
        return l;
    }
    if (auto p = getPointData(name)) {
        return p;
    }
    return std::nullopt;
}