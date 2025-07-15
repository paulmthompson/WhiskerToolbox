# TableView Data Access Layer Implementation

This directory contains the foundational components for the TableView system's data access layer, as described in the design document.

## Files Created

### Core Interfaces
- **IAnalogSource.h** - Abstract interface for unified data access
- **IRowSelector.h** - Interface for row definition with concrete implementations
- **ExecutionPlan.h** - Data structure for cached access patterns

### Data Access Adapters
- **PointComponentAdapter.h/cpp** - Adapter for extracting x/y components from PointData
- **AnalogDataAdapter.h/cpp** - Adapter for AnalogTimeSeries data
- **DataManagerExtension.h/cpp** - Factory extension for DataManager

### Column Computation Strategies
- **IColumnComputer.h** - Abstract interface for column computation strategies
- **IntervalReductionComputer.h/cpp** - Computer for reduction operations over intervals

### Core TableView System
- **Column.h/cpp** - Individual column with lazy evaluation and caching
- **TableView.h/cpp** - Main orchestrator with dependency management
- **TableViewBuilder.h/cpp** - Fluent builder pattern for table construction

### Example Usage
- **example_usage.cpp** - Demonstrates how to use the data access layer
- **interval_reduction_example.cpp** - Demonstrates interval reduction computations
- **tableview_example.cpp** - Comprehensive demonstration of the complete TableView system

### TableView System
- **Lazy Evaluation**: Columns are only computed when first accessed
- **Intelligent Caching**: Both column data and execution plans are cached
- **Dependency Management**: Handles column dependencies and prevents circular references
- **Builder Pattern**: Fluent API for table construction
- **ExecutionPlan Optimization**: Expensive time-to-index conversions are cached and reused

### Column Management
- **State Tracking**: Uses std::variant to track materialized vs unmaterialized state
- **Automatic Materialization**: Triggers computation on first access
- **Dependency Resolution**: Ensures dependent columns are materialized first
- **Cache Invalidation**: Supports clearing cache for recomputation

## Key Features

### PointComponentAdapter
- Lazy materialization of point component data
- Converts Point2D<float> to contiguous double arrays
- Supports both X and Y components
- Time-ordered data extraction

### AnalogDataAdapter  
- Bridges existing AnalogTimeSeries to IAnalogSource interface
- Converts float data to double for consistency
- Provides efficient span-based access

### DataManagerExtension
- Factory pattern implementation for getAnalogSource()
- Handles both physical data ("LFP") and virtual data ("MyPoints.x")
- Caching for performance
- Regular expression parsing for virtual source names

## Usage Examples

### Physical Data Access
```cpp
DataManager dataManager;
DataManagerExtension dmExtension(dataManager);

// Get analog source for physical data
auto lfpSource = dmExtension.getAnalogSource("LFP");
auto dataSpan = lfpSource->getDataSpan();
```

### Virtual Data Access
```cpp
// Get X component of point data
auto spikesX = dmExtension.getAnalogSource("Spikes.x");
auto xValues = spikesX->getDataSpan();

// Get Y component of point data  
auto spikesY = dmExtension.getAnalogSource("Spikes.y");
auto yValues = spikesY->getDataSpan();
```

### Complete TableView Usage
```cpp
// Create data manager and builder
DataManager dataManager;
auto dmExtension = std::make_shared<DataManagerExtension>(dataManager);
TableViewBuilder builder(dmExtension);

// Define rows using intervals
std::vector<TimeFrameInterval> intervals = {
    {TimeFrameIndex(0), TimeFrameIndex(10)},
    {TimeFrameIndex(20), TimeFrameIndex(30)}
};
builder.setRowSelector(std::make_unique<IntervalSelector>(intervals));

// Add columns with different strategies
auto source = dmExtension->getAnalogSource("LFP");
builder.addColumn("LFP_Mean", 
    std::make_unique<IntervalReductionComputer>(source, ReductionType::Mean, "LFP"));
builder.addColumn("LFP_Max", 
    std::make_unique<IntervalReductionComputer>(source, ReductionType::Max, "LFP"));

// Build and use the table
TableView table = builder.build();
auto meanData = table.getColumnSpan("LFP_Mean");  // Triggers lazy computation
auto maxData = table.getColumnSpan("LFP_Max");    // Reuses cached ExecutionPlan
```

### Interval Reduction Operations
```cpp
// Create intervals for reduction
std::vector<TimeFrameInterval> intervals = {
    {TimeFrameIndex(0), TimeFrameIndex(10)},
    {TimeFrameIndex(20), TimeFrameIndex(30)}
};
ExecutionPlan plan(intervals);

// Create reduction computers
IntervalReductionComputer meanComputer(source, ReductionType::Mean, "LFP");
IntervalReductionComputer maxComputer(source, ReductionType::Max, "LFP");

// Compute reductions
auto meanValues = meanComputer.compute(plan);
auto maxValues = maxComputer.compute(plan);
```

## Integration Notes

- The implementation uses std::span for efficient data access
- Lazy evaluation ensures data is only materialized when requested
- Caching prevents repeated expensive operations
- Error handling with appropriate fallbacks for missing data
- Compatible with existing DataManager infrastructure

## Next Steps

This data access layer and column computation strategies provide the foundation for:
1. DirectAccessComputer and TransformComputer implementations
2. TableView and Column classes with lazy evaluation
3. TableViewBuilder for fluent construction
4. ExecutionPlan generation and caching system
5. Full TableView workflow integration

The interfaces are designed to be extensible, allowing new data types, adapters, and computation strategies to be added without modifying core TableView logic.
