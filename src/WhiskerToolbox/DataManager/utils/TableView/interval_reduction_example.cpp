#include "IntervalReductionComputer.h"
#include "DataManagerExtension.h"
#include "ExecutionPlan.h"

#include <iostream>
#include <vector>

/**
 * @brief Example demonstrating the usage of IntervalReductionComputer.
 * 
 * This example shows how to:
 * 1. Create IntervalReductionComputer instances for different reduction types
 * 2. Use ExecutionPlan with intervals
 * 3. Compute reductions over intervals
 */
void demonstrateIntervalReductionComputer() {
    std::cout << "=== IntervalReductionComputer Example ===" << std::endl;
    
    // Create a DataManager and extension
    DataManager dataManager;
    DataManagerExtension dmExtension(dataManager);
    
    // Create some sample analog data
    std::vector<float> analogValues = {1.0f, 2.0f, 3.0f, 4.0f, 5.0f, 6.0f, 7.0f, 8.0f, 9.0f, 10.0f};
    std::vector<TimeFrameIndex> timeIndices;
    for (size_t i = 0; i < analogValues.size(); ++i) {
        timeIndices.emplace_back(static_cast<int64_t>(i));
    }
    
    // Create AnalogTimeSeries and add to DataManager
    auto analogData = std::make_shared<AnalogTimeSeries>(analogValues, timeIndices);
    dataManager.setData<AnalogTimeSeries>("TestSignal", analogData);
    
    // Get the analog source
    auto testSource = dmExtension.getAnalogSource("TestSignal");
    if (!testSource) {
        std::cout << "Failed to get test source!" << std::endl;
        return;
    }
    
    std::cout << "Created test signal with " << testSource->size() << " samples" << std::endl;
    
    // Create some test intervals
    // Interval 1: indices 0-2 (values 1,2,3)
    // Interval 2: indices 3-5 (values 4,5,6)  
    // Interval 3: indices 6-9 (values 7,8,9,10)
    std::vector<TimeFrameInterval> intervals = {
        {TimeFrameIndex(0), TimeFrameIndex(2)},
        {TimeFrameIndex(3), TimeFrameIndex(5)},
        {TimeFrameIndex(6), TimeFrameIndex(9)}
    };
    
    // Create ExecutionPlan with intervals
    ExecutionPlan plan(intervals);
    
    // === Test different reduction types ===
    
    // Mean reduction
    std::cout << "\n--- Mean Reduction ---" << std::endl;
    IntervalReductionComputer meanComputer(testSource, ReductionType::Mean, "TestSignal");
    auto meanResults = meanComputer.compute(plan);
    std::cout << "Source dependency: " << meanComputer.getSourceDependency() << std::endl;
    for (size_t i = 0; i < meanResults.size(); ++i) {
        std::cout << "Interval " << i << " mean: " << meanResults[i] << std::endl;
    }
    // Expected: [2.0, 5.0, 8.5]
    
    // Max reduction
    std::cout << "\n--- Max Reduction ---" << std::endl;
    IntervalReductionComputer maxComputer(testSource, ReductionType::Max, "TestSignal");
    auto maxResults = maxComputer.compute(plan);
    for (size_t i = 0; i < maxResults.size(); ++i) {
        std::cout << "Interval " << i << " max: " << maxResults[i] << std::endl;
    }
    // Expected: [3.0, 6.0, 10.0]
    
    // Min reduction
    std::cout << "\n--- Min Reduction ---" << std::endl;
    IntervalReductionComputer minComputer(testSource, ReductionType::Min, "TestSignal");
    auto minResults = minComputer.compute(plan);
    for (size_t i = 0; i < minResults.size(); ++i) {
        std::cout << "Interval " << i << " min: " << minResults[i] << std::endl;
    }
    // Expected: [1.0, 4.0, 7.0]
    
    // Standard deviation reduction
    std::cout << "\n--- StdDev Reduction ---" << std::endl;
    IntervalReductionComputer stdDevComputer(testSource, ReductionType::StdDev, "TestSignal");
    auto stdDevResults = stdDevComputer.compute(plan);
    for (size_t i = 0; i < stdDevResults.size(); ++i) {
        std::cout << "Interval " << i << " stddev: " << stdDevResults[i] << std::endl;
    }
    
    // Sum reduction
    std::cout << "\n--- Sum Reduction ---" << std::endl;
    IntervalReductionComputer sumComputer(testSource, ReductionType::Sum, "TestSignal");
    auto sumResults = sumComputer.compute(plan);
    for (size_t i = 0; i < sumResults.size(); ++i) {
        std::cout << "Interval " << i << " sum: " << sumResults[i] << std::endl;
    }
    // Expected: [6.0, 15.0, 34.0]
    
    // Count reduction
    std::cout << "\n--- Count Reduction ---" << std::endl;
    IntervalReductionComputer countComputer(testSource, ReductionType::Count, "TestSignal");
    auto countResults = countComputer.compute(plan);
    for (size_t i = 0; i < countResults.size(); ++i) {
        std::cout << "Interval " << i << " count: " << countResults[i] << std::endl;
    }
    // Expected: [3.0, 3.0, 4.0]
    
    // === Test edge cases ===
    std::cout << "\n--- Edge Cases ---" << std::endl;
    
    // Test with empty interval (should handle gracefully)
    std::vector<TimeFrameInterval> emptyIntervals = {};
    ExecutionPlan emptyPlan(emptyIntervals);
    auto emptyResults = meanComputer.compute(emptyPlan);
    std::cout << "Empty intervals result size: " << emptyResults.size() << std::endl;
    
    // Test with out-of-bounds interval
    std::vector<TimeFrameInterval> outOfBoundsIntervals = {
        {TimeFrameIndex(15), TimeFrameIndex(20)} // Beyond data range
    };
    ExecutionPlan outOfBoundsPlan(outOfBoundsIntervals);
    auto outOfBoundsResults = meanComputer.compute(outOfBoundsPlan);
    std::cout << "Out-of-bounds result: " << outOfBoundsResults[0] << " (should be NaN)" << std::endl;
    
    // Test with point data components
    std::cout << "\n--- Point Data Components ---" << std::endl;
    
    // Create some sample point data
    auto pointData = std::make_shared<PointData>();
    pointData->addAtTime(TimeFrameIndex(0), Point2D<float>{1.0f, 10.0f}, false);
    pointData->addAtTime(TimeFrameIndex(1), Point2D<float>{2.0f, 20.0f}, false);
    pointData->addAtTime(TimeFrameIndex(2), Point2D<float>{3.0f, 30.0f}, false);
    pointData->addAtTime(TimeFrameIndex(3), Point2D<float>{4.0f, 40.0f}, false);
    pointData->addAtTime(TimeFrameIndex(4), Point2D<float>{5.0f, 50.0f}, false);
    
    dataManager.setData<PointData>("TestPoints", pointData);
    
    // Get X component
    auto pointXSource = dmExtension.getAnalogSource("TestPoints.x");
    if (pointXSource) {
        IntervalReductionComputer pointXMeanComputer(pointXSource, ReductionType::Mean, "TestPoints.x");
        
        // Create intervals for point data
        std::vector<TimeFrameInterval> pointIntervals = {
            {TimeFrameIndex(0), TimeFrameIndex(2)}, // X values: 1,2,3
            {TimeFrameIndex(3), TimeFrameIndex(4)}  // X values: 4,5
        };
        ExecutionPlan pointPlan(pointIntervals);
        
        auto pointXResults = pointXMeanComputer.compute(pointPlan);
        std::cout << "Point X mean results:" << std::endl;
        for (size_t i = 0; i < pointXResults.size(); ++i) {
            std::cout << "  Interval " << i << ": " << pointXResults[i] << std::endl;
        }
        // Expected: [2.0, 4.5]
    }
}

// Uncomment to run the example
// int main() {
//     demonstrateIntervalReductionComputer();
//     return 0;
// }
