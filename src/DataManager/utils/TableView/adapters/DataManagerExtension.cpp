#include "DataManagerExtension.h"

#include "DataManager.hpp"
#include "utils/TableView/adapters/AnalogDataAdapter.h"
#include "utils/TableView/adapters/DigitalEventDataAdapter.h"
#include "utils/TableView/adapters/DigitalIntervalDataAdapter.h"
#include "utils/TableView/adapters/LineDataAdapter.h"
#include "utils/TableView/adapters/PointDataAdapter.h"
#include "utils/TableView/interfaces/IAnalogSource.h"
#include "utils/TableView/interfaces/IEventSource.h"
#include "utils/TableView/interfaces/IIntervalSource.h"
#include "utils/TableView/interfaces/ILineSource.h"
#include "utils/TableView/interfaces/IPointSource.h"


#include <iostream>

// Define the static regex
std::regex const DataManagerExtension::s_virtualSourceRegex(R"(^(\w+)\.(x|y)$)");

DataManagerExtension::DataManagerExtension(DataManager & dataManager)
    : m_dataManager(dataManager) {
}

std::shared_ptr<IAnalogSource> DataManagerExtension::getAnalogSource(std::string const & name) {
    // Check cache first
    auto cacheIt = m_dataSourceCache.find(name);
    if (cacheIt != m_dataSourceCache.end()) {
        return cacheIt->second;
    }

    std::shared_ptr<IAnalogSource> source = nullptr;

    // Try to parse as virtual source first (e.g., "MyPoints.x")
    auto [pointDataName, component] = parseVirtualSourceName(name);
    if (!pointDataName.empty()) {
        source = createPointComponentAdapter(pointDataName, component);
    } else {
        // Try as physical analog data
        source = createAnalogDataAdapter(name);
    }

    // Cache the result (even if nullptr) to avoid repeated lookups
    m_dataSourceCache[name] = source;
    return source;
}

void DataManagerExtension::clearCache() {
    m_dataSourceCache.clear();
    m_eventSourceCache.clear();
    m_intervalSourceCache.clear();
    m_lineSourceCache.clear();
    m_pointSourceCache.clear();
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

std::shared_ptr<IPointSource> DataManagerExtension::createPointDataAdapter(std::string const & name) {
    try {
        auto pointData = m_dataManager.getData<PointData>(name);
        if (!pointData) {
            return nullptr;
        }

        auto timeFrame_key = m_dataManager.getTimeKey(name);
        auto timeFrame = m_dataManager.getTime(timeFrame_key);

        return std::make_shared<PointDataAdapter>(pointData, timeFrame, name);
    } catch (std::exception const & e) {
        std::cerr << "Error creating PointDataAdapter for '" << name << "': " << e.what() << std::endl;
        return nullptr;
    }
}


std::shared_ptr<IAnalogSource> DataManagerExtension::createPointComponentAdapter(
        std::string const & pointDataName,
        PointComponentAdapter::Component component) {

    try {
        auto pointData = m_dataManager.getData<PointData>(pointDataName);
        if (!pointData) {
            return nullptr;
        }

        // Check if PointData has multiple samples per timestamp
        // PointComponentAdapter can only handle single-sample data
        auto pointAdapter = std::make_shared<PointDataAdapter>(pointData, nullptr, pointDataName);
        if (pointAdapter->hasMultiSamples()) {
            std::cerr << "Cannot create PointComponentAdapter for '" << pointDataName 
                      << "': PointData has multiple samples per timestamp. "
                      << "Use PointDataAdapter with IPointSource interface instead." << std::endl;
            return nullptr;
        }

        auto timeFrame_key = m_dataManager.getTimeKey(pointDataName);
        auto timeFrame = m_dataManager.getTime(timeFrame_key);

        // Create the full name for the virtual source
        std::string fullName = pointDataName + (component == PointComponentAdapter::Component::X ? ".x" : ".y");

        return std::make_shared<PointComponentAdapter>(pointData, component, timeFrame, fullName);
    } catch (std::exception const & e) {
        std::cerr << "Error creating PointComponentAdapter for '" << pointDataName
                  << "' component: " << e.what() << std::endl;
        return nullptr;
    }
}

std::pair<std::string, PointComponentAdapter::Component>
DataManagerExtension::parseVirtualSourceName(std::string const & name) {
    std::smatch match;
    if (!std::regex_match(name, match, s_virtualSourceRegex)) {
        return {"", PointComponentAdapter::Component::X};// Invalid format
    }

    std::string dataName = match[1].str();
    std::string componentStr = match[2].str();

    PointComponentAdapter::Component component = (componentStr == "x") ? PointComponentAdapter::Component::X : PointComponentAdapter::Component::Y;

    return {dataName, component};
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

std::shared_ptr<IPointSource> DataManagerExtension::getPointSource(std::string const & name) {
    // Check cache first
    auto it = m_pointSourceCache.find(name);
    if (it != m_pointSourceCache.end()) {
        return it->second;
    }

    // Create a new point source adapter
    auto pointSource = createPointDataAdapter(name);
    if (pointSource) {
        m_pointSourceCache[name] = pointSource;
    } else {
        std::cerr << "Point source '" << name << "' not found." << std::endl;
    }
    return pointSource;
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
    if (auto p = getPointSource(name)) {
        return p;
    }
    return std::nullopt;
}