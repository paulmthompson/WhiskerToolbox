#include "DataManager/utils/TableView/interfaces/ILineSource.h"
#include "DataManager/utils/TableView/adapters/LineDataAdapter.h"
#include "DataManager/Lines/Line_Data.hpp"
#include "DataManager/TimeFrame.hpp"
#include "CoreGeometry/lines.hpp"
#include "CoreGeometry/points.hpp"

#include <cassert>
#include <iostream>
#include <memory>

/**
 * @brief Simple test for ILineSource interface and LineDataAdapter.
 * 
 * This test verifies that:
 * 1. LineDataAdapter can be created with LineData
 * 2. The adapter correctly implements the ILineSource interface
 * 3. Basic functionality like getName(), getTimeFrame(), size() works
 * 4. getLines() and getLinesInRange() return expected data
 */
int main() {
    std::cout << "Testing ILineSource interface and LineDataAdapter..." << std::endl;

    // Create a TimeFrame
    auto timeFrame = std::make_shared<TimeFrame>(0.0f, 10.0f, 11);

    // Create some test line data
    auto lineData = std::make_shared<LineData>();
    
    // Add some test lines at different times
    std::vector<Point2D<float>> line1 = {
        {1.0f, 2.0f},
        {3.0f, 4.0f},
        {5.0f, 6.0f}
    };
    
    std::vector<Point2D<float>> line2 = {
        {7.0f, 8.0f},
        {9.0f, 10.0f}
    };

    lineData->addAtTime(TimeFrameIndex(0), Line2D(line1));
    lineData->addAtTime(TimeFrameIndex(1), Line2D(line2));
    lineData->setTimeFrame(timeFrame);

    // Create the adapter
    auto adapter = std::make_shared<LineDataAdapter>(lineData, timeFrame, "TestLines");

    // Test basic interface methods
    assert(adapter->getName() == "TestLines");
    assert(adapter->getTimeFrame() == timeFrame);
    assert(adapter->size() == 2); // Two lines total

    // Test getLines()
    auto allLines = adapter->getLines();
    assert(allLines.size() == 2);
    assert(allLines[0].size() == 3); // First line has 3 points
    assert(allLines[1].size() == 2); // Second line has 2 points

    // Test getLinesInRange()
    auto linesInRange = adapter->getLinesInRange(TimeFrameIndex(0), TimeFrameIndex(0), timeFrame.get());
    assert(linesInRange.size() == 1); // Only one line at time 0
    assert(linesInRange[0].size() == 3); // First line has 3 points

    linesInRange = adapter->getLinesInRange(TimeFrameIndex(1), TimeFrameIndex(1), timeFrame.get());
    assert(linesInRange.size() == 1); // Only one line at time 1
    assert(linesInRange[1].size() == 2); // Second line has 2 points

    // Test range spanning multiple times
    linesInRange = adapter->getLinesInRange(TimeFrameIndex(0), TimeFrameIndex(1), timeFrame.get());
    assert(linesInRange.size() == 2); // Both lines in range

    std::cout << "All tests passed!" << std::endl;
    return 0;
} 