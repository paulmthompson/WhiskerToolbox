#include "TableView.h"
#include "TableViewBuilder.h"
#include "IntervalReductionComputer.h"
#include "EventInIntervalComputer.h"
#include "DataManagerExtension.h"
#include "IRowSelector.h"

#include <iostream>
#include <memory>
#include <vector>

/**
 * @brief Example demonstrating the heterogeneous TableView system.
 * 
 * This example shows how to use the updated TableView system with
 * heterogeneous column types including double, bool, int, and vector types.
 */
void heterogeneous_tableview_example() {
    try {
        // Create a mock DataManager (in real usage, this would be your actual DataManager)
        DataManager dataManager;
        auto dataManagerExtension = std::make_shared<DataManagerExtension>(dataManager);
        
        // Create example intervals for the table rows
        std::vector<Interval> intervals = {
            {0, 100},
            {200, 300},
            {400, 500},
            {600, 700}
        };
        
        // Create a TableView builder
        TableViewBuilder builder(dataManagerExtension);
        
        // Set the row selector (defines what constitutes a "row")
        builder.setRowSelector(std::make_unique<IntervalSelector>(intervals));
        
        // --- Example 1: Traditional double columns ---
        
        // Get analog sources from the DataManager
        auto lfpSource = dataManagerExtension->getAnalogSource("LFP");
        auto spikeXSource = dataManagerExtension->getAnalogSource("Spikes.x");
        
        if (lfpSource && spikeXSource) {
            // Add double columns using the traditional approach
            builder.addColumn("LFP_Mean", 
                std::make_unique<IntervalReductionComputer>(lfpSource, ReductionType::Mean));
            
            builder.addColumn("LFP_StdDev", 
                std::make_unique<IntervalReductionComputer>(lfpSource, ReductionType::StdDev));
            
            builder.addColumn("SpikeX_Max", 
                std::make_unique<IntervalReductionComputer>(spikeXSource, ReductionType::Max));
        }
        
        // --- Example 2: Heterogeneous columns with event data ---
        
        // Get an event source for digital events
        auto eventSource = dataManagerExtension->getEventSource("MyEvents");
        
        if (eventSource) {
            // Add a boolean column for event presence
            builder.addColumn<bool>("HasEvents", 
                std::make_unique<EventInIntervalComputer<bool>>(
                    eventSource, EventOperation::Presence, "MyEvents"));
            
            // Add an integer column for event count
            builder.addColumn<int>("EventCount", 
                std::make_unique<EventInIntervalComputer<int>>(
                    eventSource, EventOperation::Count, "MyEvents"));
            
            // Add a vector column for gathered events
            builder.addColumn<std::vector<TimeFrameIndex>>("GatheredEvents", 
                std::make_unique<EventInIntervalComputer<std::vector<TimeFrameIndex>>>(
                    eventSource, EventOperation::Gather, "MyEvents"));
        }
        
        // Build the table
        TableView table = builder.build();
        
        // --- Example 3: Type-safe data access ---
        
        std::cout << "Table has " << table.getRowCount() << " rows and " 
                  << table.getColumnCount() << " columns." << std::endl;
        
        // Access double columns (traditional way still works)
        if (table.hasColumn("LFP_Mean")) {
            auto lfpMeans = table.getColumnSpan("LFP_Mean");
            std::cout << "LFP means: ";
            for (size_t i = 0; i < lfpMeans.size() && i < 5; ++i) {
                std::cout << lfpMeans[i] << " ";
            }
            std::cout << std::endl;
        }
        
        // Access double columns using the new typed interface
        if (table.hasColumn("LFP_StdDev")) {
            const auto& lfpStdDevs = table.getColumnValues<double>("LFP_StdDev");
            std::cout << "LFP standard deviations: ";
            for (size_t i = 0; i < lfpStdDevs.size() && i < 5; ++i) {
                std::cout << lfpStdDevs[i] << " ";
            }
            std::cout << std::endl;
        }
        
        // Access boolean columns
        if (table.hasColumn("HasEvents")) {
            const auto& hasEvents = table.getColumnValues<bool>("HasEvents");
            std::cout << "Event presence: ";
            for (size_t i = 0; i < hasEvents.size() && i < 5; ++i) {
                std::cout << (hasEvents[i] ? "true" : "false") << " ";
            }
            std::cout << std::endl;
        }
        
        // Access integer columns
        if (table.hasColumn("EventCount")) {
            const auto& eventCounts = table.getColumnValues<int>("EventCount");
            std::cout << "Event counts: ";
            for (size_t i = 0; i < eventCounts.size() && i < 5; ++i) {
                std::cout << eventCounts[i] << " ";
            }
            std::cout << std::endl;
        }
        
        // Access vector columns
        if (table.hasColumn("GatheredEvents")) {
            const auto& gatheredEvents = table.getColumnValues<std::vector<TimeFrameIndex>>("GatheredEvents");
            std::cout << "Gathered events for first interval: ";
            if (!gatheredEvents.empty()) {
                const auto& firstIntervalEvents = gatheredEvents[0];
                for (size_t i = 0; i < firstIntervalEvents.size() && i < 5; ++i) {
                    std::cout << firstIntervalEvents[i] << " ";
                }
            }
            std::cout << std::endl;
        }
        
        // --- Example 4: Type safety demonstration ---
        
        // This would work fine
        try {
            const auto& doubleColumn = table.getColumnValues<double>("LFP_Mean");
            std::cout << "Successfully accessed double column" << std::endl;
        } catch (const std::exception& e) {
            std::cout << "Error accessing double column: " << e.what() << std::endl;
        }
        
        // This would throw an exception due to type mismatch
        try {
            const auto& wrongType = table.getColumnValues<int>("LFP_Mean");
            std::cout << "This line should not be reached" << std::endl;
        } catch (const std::exception& e) {
            std::cout << "Expected type mismatch error: " << e.what() << std::endl;
        }
        
        // --- Example 5: Performance considerations ---
        
        // The lazy evaluation and caching still work with heterogeneous types
        std::cout << "\\nPerformance test:" << std::endl;
        
        // First access triggers computation
        auto start = std::chrono::high_resolution_clock::now();
        if (table.hasColumn("LFP_Mean")) {
            const auto& data1 = table.getColumnValues<double>("LFP_Mean");
            auto end = std::chrono::high_resolution_clock::now();
            auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end - start);
            std::cout << "First access (with computation): " << duration.count() << " μs" << std::endl;
            
            // Second access uses cached data
            start = std::chrono::high_resolution_clock::now();
            const auto& data2 = table.getColumnValues<double>("LFP_Mean");
            end = std::chrono::high_resolution_clock::now();
            duration = std::chrono::duration_cast<std::chrono::microseconds>(end - start);
            std::cout << "Second access (cached): " << duration.count() << " μs" << std::endl;
        }
        
    } catch (const std::exception& e) {
        std::cerr << "Error in heterogeneous TableView example: " << e.what() << std::endl;
    }
}

/**
 * @brief Example showing how to extend the system with custom types.
 * 
 * This demonstrates how developers can create their own column computers
 * for custom data types.
 */
void custom_type_example() {
    // Custom data type example
    struct CustomData {
        double value;
        std::string label;
        bool isValid;
    };
    
    // You could create a CustomColumnComputer<CustomData> that implements
    // IColumnComputer<CustomData> to compute CustomData objects for each row
    
    // Then use it like:
    // builder.addColumn<CustomData>("CustomColumn", 
    //     std::make_unique<CustomColumnComputer<CustomData>>(parameters));
    
    // And access it like:
    // const auto& customData = table.getColumnValues<CustomData>("CustomColumn");
    
    std::cout << "Custom type support is available for any type T that can be stored in std::vector<T>" << std::endl;
}

int main() {
    std::cout << "=== Heterogeneous TableView System Example ===" << std::endl;
    
    heterogeneous_tableview_example();
    
    std::cout << "\\n=== Custom Type Extension Example ===" << std::endl;
    custom_type_example();
    
    return 0;
}
