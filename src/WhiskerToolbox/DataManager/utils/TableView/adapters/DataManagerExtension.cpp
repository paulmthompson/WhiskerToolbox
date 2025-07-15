#include "DataManagerExtension.h"

#include <iostream>

// Define the static regex
const std::regex DataManagerExtension::s_virtualSourceRegex(R"(^(\w+)\.(x|y)$)");

DataManagerExtension::DataManagerExtension(DataManager& dataManager)
    : m_dataManager(dataManager)
{
}

std::shared_ptr<IAnalogSource> DataManagerExtension::getAnalogSource(const std::string& name) {
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
}

std::shared_ptr<IAnalogSource> DataManagerExtension::createAnalogDataAdapter(const std::string& name) {
    try {
        auto analogData = m_dataManager.getData<AnalogTimeSeries>(name);
        if (!analogData) {
            return nullptr;
        }

        // Use a default timeframe ID of 0 for now
        // In a real implementation, this would be derived from the data or DataManager
        int timeFrameId = 0;
        
        return std::make_shared<AnalogDataAdapter>(analogData, timeFrameId, name);
    } catch (const std::exception& e) {
        std::cerr << "Error creating AnalogDataAdapter for '" << name << "': " << e.what() << std::endl;
        return nullptr;
    }
}

std::shared_ptr<IAnalogSource> DataManagerExtension::createPointComponentAdapter(
    const std::string& pointDataName, 
    PointComponentAdapter::Component component) {
    
    try {
        auto pointData = m_dataManager.getData<PointData>(pointDataName);
        if (!pointData) {
            return nullptr;
        }

        // Use a default timeframe ID of 0 for now
        // In a real implementation, this would be derived from the data or DataManager
        int timeFrameId = 0;
        
        // Create the full name for the virtual source
        std::string fullName = pointDataName + (component == PointComponentAdapter::Component::X ? ".x" : ".y");
        
        return std::make_shared<PointComponentAdapter>(pointData, component, timeFrameId, fullName);
    } catch (const std::exception& e) {
        std::cerr << "Error creating PointComponentAdapter for '" << pointDataName 
                  << "' component: " << e.what() << std::endl;
        return nullptr;
    }
}

std::pair<std::string, PointComponentAdapter::Component> 
DataManagerExtension::parseVirtualSourceName(const std::string& name) {
    std::smatch match;
    if (!std::regex_match(name, match, s_virtualSourceRegex)) {
        return {"", PointComponentAdapter::Component::X}; // Invalid format
    }

    std::string dataName = match[1].str();
    std::string componentStr = match[2].str();

    PointComponentAdapter::Component component = (componentStr == "x") ? 
        PointComponentAdapter::Component::X : 
        PointComponentAdapter::Component::Y;

    return {dataName, component};
}

std::shared_ptr<IEventSource> DataManagerExtension::getEventSource(const std::string& name) {
    // Check cache first
    auto it = m_eventSourceCache.find(name);
    if (it != m_eventSourceCache.end()) {
        return it->second;
    }

    // For now, return nullptr as we don't have concrete event source implementations yet
    // This method can be extended when DigitalEventSeries is implemented
    return nullptr;
}
