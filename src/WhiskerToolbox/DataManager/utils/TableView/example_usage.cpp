#include "DataManagerExtension.h"
#include "PointComponentAdapter.h"
#include "AnalogDataAdapter.h"

#include <iostream>
#include <vector>

/**
 * @brief Example demonstrating the usage of DataManagerExtension and adapters.
 * 
 * This example shows how to:
 * 1. Create a DataManagerExtension
 * 2. Use the getAnalogSource factory method for both physical and virtual data
 * 3. Access the materialized data through the IAnalogSource interface
 */
void demonstrateTableViewDataAccess() {
    // Create a DataManager instance
    DataManager dataManager;
    
    // Create the extension
    DataManagerExtension dmExtension(dataManager);
    
    // === Example 1: Physical analog data ===
    std::cout << "=== Physical Analog Data Example ===" << std::endl;
    
    // First, let's create some sample analog data
    std::vector<float> analogValues = {1.0f, 2.5f, 3.2f, 1.8f, 4.1f};
    std::vector<TimeFrameIndex> timeIndices;
    for (size_t i = 0; i < analogValues.size(); ++i) {
        timeIndices.emplace_back(static_cast<int64_t>(i * 10)); // Every 10 time units
    }
    
    // Create AnalogTimeSeries and add to DataManager
    auto analogData = std::make_shared<AnalogTimeSeries>(analogValues, timeIndices);
    dataManager.setData<AnalogTimeSeries>("LFP", analogData);
    
    // Get the analog source through the factory
    auto lfpSource = dmExtension.getAnalogSource("LFP");
    if (lfpSource) {
        std::cout << "LFP source found!" << std::endl;
        std::cout << "  Size: " << lfpSource->size() << std::endl;
        std::cout << "  TimeFrame ID: " << lfpSource->getTimeFrameId() << std::endl;
        
        // Get the data span
        auto dataSpan = lfpSource->getDataSpan();
        std::cout << "  Data values: ";
        for (size_t i = 0; i < dataSpan.size(); ++i) {
            std::cout << dataSpan[i] << " ";
        }
        std::cout << std::endl;
    } else {
        std::cout << "LFP source not found!" << std::endl;
    }
    
    // === Example 2: Virtual point data ===
    std::cout << "\n=== Virtual Point Data Example ===" << std::endl;
    
    // Create some sample point data
    auto pointData = std::make_shared<PointData>();
    
    // Add some points at different times
    pointData->addAtTime(TimeFrameIndex(0), Point2D<float>{10.5f, 20.3f}, false);
    pointData->addAtTime(TimeFrameIndex(1), Point2D<float>{15.2f, 25.1f}, false);
    pointData->addAtTime(TimeFrameIndex(2), Point2D<float>{12.8f, 18.9f}, false);
    
    // Add to DataManager
    dataManager.setData<PointData>("Spikes", pointData);
    
    // Get the X component through the factory
    auto spikesXSource = dmExtension.getAnalogSource("Spikes.x");
    if (spikesXSource) {
        std::cout << "Spikes.x source found!" << std::endl;
        std::cout << "  Size: " << spikesXSource->size() << std::endl;
        std::cout << "  TimeFrame ID: " << spikesXSource->getTimeFrameId() << std::endl;
        
        auto dataSpan = spikesXSource->getDataSpan();
        std::cout << "  X values: ";
        for (size_t i = 0; i < dataSpan.size(); ++i) {
            std::cout << dataSpan[i] << " ";
        }
        std::cout << std::endl;
    } else {
        std::cout << "Spikes.x source not found!" << std::endl;
    }
    
    // Get the Y component through the factory
    auto spikesYSource = dmExtension.getAnalogSource("Spikes.y");
    if (spikesYSource) {
        std::cout << "Spikes.y source found!" << std::endl;
        std::cout << "  Size: " << spikesYSource->size() << std::endl;
        
        auto dataSpan = spikesYSource->getDataSpan();
        std::cout << "  Y values: ";
        for (size_t i = 0; i < dataSpan.size(); ++i) {
            std::cout << dataSpan[i] << " ";
        }
        std::cout << std::endl;
    } else {
        std::cout << "Spikes.y source not found!" << std::endl;
    }
    
    // === Example 3: Cache behavior ===
    std::cout << "\n=== Cache Behavior Example ===" << std::endl;
    
    // Second call should return the cached adapter
    auto cachedLfpSource = dmExtension.getAnalogSource("LFP");
    std::cout << "Second call to getAnalogSource('LFP'): " 
              << (cachedLfpSource == lfpSource ? "Same instance (cached)" : "Different instance") 
              << std::endl;
    
    // Clear cache and try again
    dmExtension.clearCache();
    auto newLfpSource = dmExtension.getAnalogSource("LFP");
    std::cout << "After cache clear: " 
              << (newLfpSource == lfpSource ? "Same instance" : "Different instance (new)") 
              << std::endl;
    
    // === Example 4: Invalid requests ===
    std::cout << "\n=== Invalid Request Example ===" << std::endl;
    
    auto invalidSource = dmExtension.getAnalogSource("NonExistent");
    std::cout << "Request for non-existent source: " 
              << (invalidSource ? "Found" : "Not found (expected)") << std::endl;
    
    auto invalidComponentSource = dmExtension.getAnalogSource("NonExistent.x");
    std::cout << "Request for non-existent component source: " 
              << (invalidComponentSource ? "Found" : "Not found (expected)") << std::endl;
}

// Uncomment to run the example
// int main() {
//     demonstrateTableViewDataAccess();
//     return 0;
// }
