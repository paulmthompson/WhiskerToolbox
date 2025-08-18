#include "TableView.h"
#include "TableViewBuilder.h"
#include "IntervalReductionComputer.h"
#include "DataManagerExtension.h"

#include <iostream>
#include <vector>

/**
 * @brief Comprehensive example demonstrating the full TableView system.
 * 
 * This example shows how to:
 * 1. Create a TableViewBuilder with a DataManagerExtension
 * 2. Set up row selectors and add columns with different computation strategies
 * 3. Build the TableView and access column data with lazy evaluation
 * 4. Demonstrate caching and dependency handling
 */
void demonstrateTableViewSystem() {
    std::cout << "=== TableView System Example ===" << std::endl;
    
    // Create a DataManager and extension
    DataManager dataManager;
    auto dmExtension = std::make_shared<DataManagerExtension>(dataManager);
    
    // Create some sample analog data
    std::vector<float> analogValues = {1.0f, 2.0f, 3.0f, 4.0f, 5.0f, 6.0f, 7.0f, 8.0f, 9.0f, 10.0f};
    std::vector<TimeFrameIndex> timeIndices;
    for (size_t i = 0; i < analogValues.size(); ++i) {
        timeIndices.emplace_back(static_cast<int64_t>(i));
    }
    
    // Create AnalogTimeSeries and add to DataManager
    auto analogData = std::make_shared<AnalogTimeSeries>(analogValues, timeIndices);
    dataManager.setData<AnalogTimeSeries>("TestSignal", analogData);
    
    // Create some sample point data
    auto pointData = std::make_shared<PointData>();
    pointData->addAtTime(TimeFrameIndex(0), Point2D<float>{1.0f, 10.0f}, false);
    pointData->addAtTime(TimeFrameIndex(1), Point2D<float>{2.0f, 20.0f}, false);
    pointData->addAtTime(TimeFrameIndex(2), Point2D<float>{3.0f, 30.0f}, false);
    pointData->addAtTime(TimeFrameIndex(3), Point2D<float>{4.0f, 40.0f}, false);
    pointData->addAtTime(TimeFrameIndex(4), Point2D<float>{5.0f, 50.0f}, false);
    
    dataManager.setData<PointData>("TestPoints", pointData);
    
    // === Create TableView using Builder Pattern ===
    std::cout << "\n--- Building TableView ---" << std::endl;
    
    TableViewBuilder builder(dmExtension);
    
    // Define intervals for the table rows
    std::vector<TimeFrameInterval> intervals = {
        {TimeFrameIndex(0), TimeFrameIndex(2)},  // Interval 1: indices 0-2
        {TimeFrameIndex(3), TimeFrameIndex(5)},  // Interval 2: indices 3-5
        {TimeFrameIndex(6), TimeFrameIndex(9)}   // Interval 3: indices 6-9
    };
    
    // Set row selector
    builder.setRowSelector(std::make_unique<IntervalSelector>(intervals));
    
    // Add columns with different reduction strategies
    auto testSource = dmExtension->getAnalogSource("TestSignal");
    auto pointXSource = dmExtension->getAnalogSource("TestPoints.x");
    auto pointYSource = dmExtension->getAnalogSource("TestPoints.y");
    
    builder.addColumn("Signal_Mean", 
                     std::make_unique<IntervalReductionComputer>(testSource, ReductionType::Mean, "TestSignal"));
    
    builder.addColumn("Signal_Max", 
                     std::make_unique<IntervalReductionComputer>(testSource, ReductionType::Max, "TestSignal"));
    
    builder.addColumn("Signal_StdDev", 
                     std::make_unique<IntervalReductionComputer>(testSource, ReductionType::StdDev, "TestSignal"));
    
    if (pointXSource) {
        builder.addColumn("Points_X_Mean", 
                         std::make_unique<IntervalReductionComputer>(pointXSource, ReductionType::Mean, "TestPoints.x"));
    }
    
    if (pointYSource) {
        builder.addColumn("Points_Y_Sum", 
                         std::make_unique<IntervalReductionComputer>(pointYSource, ReductionType::Sum, "TestPoints.y"));
    }
    
    // Build the TableView
    TableView table = builder.build();
    
    std::cout << "TableView created with " << table.getRowCount() << " rows and " 
              << table.getColumnCount() << " columns" << std::endl;
    
    // === Demonstrate Lazy Evaluation ===
    std::cout << "\n--- Demonstrating Lazy Evaluation ---" << std::endl;
    
    // Get column names
    auto columnNames = table.getColumnNames();
    std::cout << "Available columns: ";
    for (const auto& name : columnNames) {
        std::cout << name << " ";
    }
    std::cout << std::endl;
    
    // Access columns - this will trigger computation
    std::cout << "\nAccessing Signal_Mean column (triggers computation):" << std::endl;
    auto signalMeanData = table.getColumnSpan("Signal_Mean");
    for (size_t i = 0; i < signalMeanData.size(); ++i) {
        std::cout << "  Row " << i << ": " << signalMeanData[i] << std::endl;
    }
    // Expected: [2.0, 5.0, 8.5]
    
    std::cout << "\nAccessing Signal_Max column:" << std::endl;
    auto signalMaxData = table.getColumnSpan("Signal_Max");
    for (size_t i = 0; i < signalMaxData.size(); ++i) {
        std::cout << "  Row " << i << ": " << signalMaxData[i] << std::endl;
    }
    // Expected: [3.0, 6.0, 10.0]
    
    // Access point data columns if available
    if (table.hasColumn("Points_X_Mean")) {
        std::cout << "\nAccessing Points_X_Mean column:" << std::endl;
        auto pointXMeanData = table.getColumnSpan("Points_X_Mean");
        for (size_t i = 0; i < pointXMeanData.size(); ++i) {
            std::cout << "  Row " << i << ": " << pointXMeanData[i] << std::endl;
        }
    }
    
    // === Demonstrate Caching ===
    std::cout << "\n--- Demonstrating Caching ---" << std::endl;
    
    // Second access should be faster (cached)
    std::cout << "Second access to Signal_Mean (should be cached):" << std::endl;
    auto cachedSignalMeanData = table.getColumnSpan("Signal_Mean");
    std::cout << "Data identical: " << (cachedSignalMeanData.data() == signalMeanData.data() ? "Yes" : "No") << std::endl;
    
    // === Demonstrate materializeAll ===
    std::cout << "\n--- Demonstrating materializeAll ---" << std::endl;
    
    // Clear cache and materialize all at once
    table.clearCache();
    std::cout << "Cache cleared, materializing all columns..." << std::endl;
    table.materializeAll();
    std::cout << "All columns materialized!" << std::endl;
    
    // Verify all columns are accessible
    for (const auto& columnName : columnNames) {
        auto columnData = table.getColumnSpan(columnName);
        std::cout << "Column '" << columnName << "' has " << columnData.size() << " values" << std::endl;
    }
    
    // === Error Handling Example ===
    std::cout << "\n--- Error Handling Example ---" << std::endl;
    
    try {
        auto nonExistentColumn = table.getColumnSpan("NonExistent");
    } catch (const std::runtime_error& e) {
        std::cout << "Expected error: " << e.what() << std::endl;
    }
    
    std::cout << "\nTableView system demonstration complete!" << std::endl;
}

// Uncomment to run the example
// int main() {
//     demonstrateTableViewSystem();
//     return 0;
// }
