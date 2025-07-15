# Heterogeneous TableView System

## Overview

The TableView system has been extended to support heterogeneous column types while maintaining its core principles of lazy evaluation, caching, and dependency management. This allows tables to contain columns of different types (double, bool, int, custom types) in a type-safe manner.

## Key Components

### 1. IColumn Interface
- **Purpose**: Provides type erasure for heterogeneous column management
- **Location**: `IColumn.h`
- **Features**: 
  - Type-agnostic column operations
  - Runtime type information via `getType()`
  - Materialization interface for lazy evaluation

### 2. Templated Column<T>
- **Purpose**: Type-safe column implementation
- **Location**: `ColumnTemplated.h`
- **Features**:
  - Inherits from `IColumn` for polymorphic management
  - Stores data in `std::variant<std::monostate, std::vector<T>>`
  - Lazy evaluation with caching

### 3. Templated IColumnComputer<T>
- **Purpose**: Type-safe computation strategies
- **Location**: `IColumnComputer.h`
- **Features**:
  - Returns `std::vector<T>` instead of fixed double type
  - Supports any type that can be stored in a vector

### 4. Event-Based Data Support
- **IEventSource**: Interface for discrete event data
- **EventInIntervalComputer<T>**: Processes events within intervals
- **EventOperation**: Enum for different event operations (Presence, Count, Gather)

## Usage Examples

### Basic Heterogeneous Table
```cpp
TableViewBuilder builder(dataManagerExtension);
builder.setRowSelector(std::make_unique<IntervalSelector>(intervals));

// Double columns (traditional)
builder.addColumn("LFP_Mean", 
    std::make_unique<IntervalReductionComputer>(lfpSource, ReductionType::Mean));

// Boolean columns
builder.addColumn<bool>("HasEvents", 
    std::make_unique<EventInIntervalComputer<bool>>(
        eventSource, EventOperation::Presence, "MyEvents"));

// Integer columns
builder.addColumn<int>("EventCount", 
    std::make_unique<EventInIntervalComputer<int>>(
        eventSource, EventOperation::Count, "MyEvents"));

// Vector columns
builder.addColumn<std::vector<TimeFrameIndex>>("GatheredEvents", 
    std::make_unique<EventInIntervalComputer<std::vector<TimeFrameIndex>>>(
        eventSource, EventOperation::Gather, "MyEvents"));

TableView table = builder.build();
```

### Type-Safe Data Access
```cpp
// Access different column types
const auto& doubleData = table.getColumnValues<double>("LFP_Mean");
const auto& boolData = table.getColumnValues<bool>("HasEvents");
const auto& intData = table.getColumnValues<int>("EventCount");
const auto& vectorData = table.getColumnValues<std::vector<TimeFrameIndex>>("GatheredEvents");

// Backward compatibility for double columns
auto span = table.getColumnSpan("LFP_Mean");
```

### Type Safety
The system provides compile-time and runtime type safety:
```cpp
// This works fine
const auto& data = table.getColumnValues<double>("LFP_Mean");

// This throws std::runtime_error due to type mismatch
const auto& wrongType = table.getColumnValues<int>("LFP_Mean");
```

## Event-Based Data Processing

### EventInIntervalComputer
Processes discrete events within time intervals:

```cpp
enum class EventOperation {
    Presence,  // Returns bool: true if any events exist
    Count,     // Returns int: number of events in interval
    Gather     // Returns std::vector<TimeFrameIndex>: all events
};

// Usage
auto eventSource = dataManager.getEventSource("MyEvents");

// Check if events exist in each interval
builder.addColumn<bool>("HasEvents", 
    std::make_unique<EventInIntervalComputer<bool>>(
        eventSource, EventOperation::Presence, "MyEvents"));

// Count events in each interval
builder.addColumn<int>("EventCount", 
    std::make_unique<EventInIntervalComputer<int>>(
        eventSource, EventOperation::Count, "MyEvents"));

// Gather all events in each interval
builder.addColumn<std::vector<TimeFrameIndex>>("AllEvents", 
    std::make_unique<EventInIntervalComputer<std::vector<TimeFrameIndex>>>(
        eventSource, EventOperation::Gather, "MyEvents"));
```

## Architecture Changes

### Before (Homogeneous)
```cpp
class Column {
    std::variant<std::monostate, std::vector<double>> m_cache;
    std::unique_ptr<IColumnComputer> m_computer;
};

class TableView {
    std::vector<std::shared_ptr<Column>> m_columns;
    std::span<const double> getColumnSpan(const std::string& name);
};
```

### After (Heterogeneous)
```cpp
class IColumn {
    virtual const std::type_info& getType() const = 0;
    virtual void materialize(TableView* table) = 0;
};

template<typename T>
class Column : public IColumn {
    std::variant<std::monostate, std::vector<T>> m_cache;
    std::unique_ptr<IColumnComputer<T>> m_computer;
};

class TableView {
    std::vector<std::shared_ptr<IColumn>> m_columns;
    
    template<typename T>
    const std::vector<T>& getColumnValues(const std::string& name);
    
    // Maintained for backward compatibility
    std::span<const double> getColumnSpan(const std::string& name);
};
```

## Performance Considerations

### Lazy Evaluation
- Columns are computed only when first accessed
- Results are cached for subsequent accesses
- ExecutionPlans are shared between columns using the same data source

### Memory Efficiency
- Type erasure allows heterogeneous storage without runtime overhead
- Caching prevents recomputation
- Span-based access for contiguous data types

### Template Instantiation
Common types are explicitly instantiated in `ColumnTemplated.cpp`:
```cpp
template class Column<double>;
template class Column<bool>;
template class Column<int>;
template class Column<std::vector<TimeFrameIndex>>;
```

## Extensibility

### Custom Types
Any type `T` that can be stored in `std::vector<T>` can be used:

```cpp
struct CustomData {
    double value;
    std::string label;
    bool isValid;
};

// Create a custom computer
class CustomComputer : public IColumnComputer<CustomData> {
    std::vector<CustomData> compute(const ExecutionPlan& plan) const override {
        // Custom computation logic
    }
};

// Use it in the table
builder.addColumn<CustomData>("CustomColumn", 
    std::make_unique<CustomComputer>(parameters));
```

### New Data Sources
Implement `IEventSource` for new event-based data types:

```cpp
class MyEventSource : public IEventSource {
    int getTimeFrameId() const override;
    std::span<const TimeFrameIndex> getEvents() const override;
};
```

## Migration Guide

### From Homogeneous to Heterogeneous

1. **Existing double columns**: No changes needed, continue using `getColumnSpan()`
2. **New typed columns**: Use `getColumnValues<T>()` for type-safe access
3. **Column computers**: Update to use `IColumnComputer<T>` instead of `IColumnComputer`
4. **Builder pattern**: Use `addColumn<T>()` for non-double types

### Backward Compatibility
- All existing code using `getColumnSpan()` continues to work
- Traditional `addColumn()` method still available for double columns
- No breaking changes to the core API

## Examples

See `HeterogeneousTableViewExample.cpp` for comprehensive usage examples including:
- Mixed column types in a single table
- Type-safe data access
- Event-based data processing
- Performance characteristics
- Custom type extensions

## Files Added/Modified

### New Files
- `IColumn.h` - Type-erased column interface
- `ColumnTemplated.h/.cpp` - Templated column implementation
- `IEventSource.h` - Event source interface
- `EventInIntervalComputer.h/.cpp` - Event processing computer
- `HeterogeneousTableViewExample.cpp` - Usage examples

### Modified Files
- `IColumnComputer.h` - Now templated
- `TableView.h/.cpp` - Added `getColumnValues<T>()` method
- `TableViewBuilder.h/.cpp` - Added `addColumn<T>()` method
- `DataManagerExtension.h/.cpp` - Added `getEventSource()` method
- `IntervalReductionComputer.h` - Updated to use `IColumnComputer<double>`
- `CMakeLists.txt` - Added new source files

The system maintains all existing functionality while adding powerful heterogeneous type support and event-based data processing capabilities.
