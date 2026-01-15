# WhiskerToolbox Python Bindings

Python bindings for WhiskerToolbox data types and transforms, powered by Pybind11.

## Features

- **Interval Type**: Basic time interval with start and end points
- **DigitalIntervalSeries**: A series of time intervals representing when events are active
- **Transform System**: Apply data transforms to interval series, including custom Python transforms
- **Boolean Operations**: Perform AND, OR, XOR, NOT, and AND_NOT operations on interval series

## Installation

The Python bindings are built as part of the WhiskerToolbox CMake build process. After building:

```bash
# Set PYTHONPATH to include the build directory
export PYTHONPATH=/path/to/WhiskerToolbox/build/src/PythonBindings:$PYTHONPATH

# Or install to Python site-packages
cmake --build build --target install
```

## Usage Examples

### Creating and Working with Intervals

```python
import whiskertoolbox as wt

# Create intervals
interval1 = wt.Interval(0, 10)
interval2 = wt.Interval(5, 15)

# Check properties
print(f"Start: {interval1.start}, End: {interval1.end}")

# Check if intervals overlap
if wt.is_overlapping(interval1, interval2):
    print("Intervals overlap!")

# Check if a time is within an interval
if wt.is_contained(interval1, 7):
    print("Time 7 is in interval1")
```

### Working with DigitalIntervalSeries

```python
import whiskertoolbox as wt

# Create an empty series
series = wt.DigitalIntervalSeries()

# Add intervals
series.add_event(wt.Interval(0, 10))
series.add_event(wt.Interval(20, 30))
series.add_event(wt.Interval(40, 50))

print(f"Series has {len(series)} intervals")

# Create from a list of intervals
intervals = [wt.Interval(0, 5), wt.Interval(10, 15), wt.Interval(20, 25)]
series2 = wt.DigitalIntervalSeries(intervals)

# Create from tuples (start, end)
series3 = wt.DigitalIntervalSeries([(0.0, 5.0), (10.0, 15.0), (20.0, 25.0)])

# Create from boolean vector
bool_vector = [True, True, False, False, True, True, True, False, False, True]
series4 = wt.DigitalIntervalSeries()
series4.create_intervals_from_bool(bool_vector)
# Creates intervals: [0,1], [4,6], [9,9]

# Get all intervals
for interval in series.get_intervals():
    print(f"Interval: {interval.start} to {interval.end}")
```

### Applying Boolean Operations

```python
import whiskertoolbox as wt

# Create two series
series1 = wt.DigitalIntervalSeries([wt.Interval(0, 10), wt.Interval(20, 30)])
series2 = wt.DigitalIntervalSeries([wt.Interval(5, 15), wt.Interval(25, 35)])

# AND operation - find overlapping intervals
params_and = wt.BooleanParams()
params_and.operation = wt.BooleanOperation.AND
params_and.other_series = series2
result_and = wt.apply_boolean_operation(series1, params_and)

# OR operation - union of intervals
params_or = wt.BooleanParams()
params_or.operation = wt.BooleanOperation.OR
params_or.other_series = series2
result_or = wt.apply_boolean_operation(series1, params_or)

# NOT operation - invert intervals
params_not = wt.BooleanParams()
params_not.operation = wt.BooleanOperation.NOT
result_not = wt.apply_boolean_operation(series1, params_not)

# AND_NOT operation - subtract series2 from series1
params_and_not = wt.BooleanParams()
params_and_not.operation = wt.BooleanOperation.AND_NOT
params_and_not.other_series = series2
result_and_not = wt.apply_boolean_operation(series1, params_and_not)
```

### Creating Custom Python Transforms

```python
import whiskertoolbox as wt

def filter_short_intervals(series, min_length=5):
    """Remove intervals shorter than min_length."""
    result = wt.DigitalIntervalSeries()
    for interval in series.get_intervals():
        length = interval.end - interval.start + 1
        if length >= min_length:
            result.add_event(interval)
    return result

def merge_nearby_intervals(series, max_gap=3):
    """Merge intervals that are close together."""
    intervals = list(series.get_intervals())
    if len(intervals) == 0:
        return wt.DigitalIntervalSeries()
    
    # Sort intervals by start time
    intervals.sort(key=lambda x: x.start)
    
    result = wt.DigitalIntervalSeries()
    current = intervals[0]
    
    for i in range(1, len(intervals)):
        next_interval = intervals[i]
        gap = next_interval.start - current.end
        
        if gap <= max_gap:
            # Merge intervals
            current = wt.Interval(current.start, next_interval.end)
        else:
            # Add current and start new one
            result.add_event(current)
            current = next_interval
    
    # Add the last interval
    result.add_event(current)
    return result

# Create a transform
transform = wt.PythonTransform("FilterShortIntervals", 
                               lambda s: filter_short_intervals(s, min_length=10))

# Use the transforms
series = wt.DigitalIntervalSeries([
    wt.Interval(0, 2),    # Short - will be filtered
    wt.Interval(10, 20),  # Long - will be kept
    wt.Interval(25, 35),  # Long - will be kept
])

filtered = filter_short_intervals(series, min_length=5)
print(f"Filtered series: {filtered}")

merged = merge_nearby_intervals(filtered, max_gap=10)
print(f"Merged series: {merged}")
```

## Data Types Reference

### Interval
A simple struct representing a time interval.

**Properties:**
- `start` (int64): Start time of the interval
- `end` (int64): End time of the interval

**Methods:**
- `__init__(start: int, end: int)`: Create an interval
- `__eq__()`: Check equality
- `__lt__()`: Compare by start time

### DigitalIntervalSeries
A series of time intervals.

**Methods:**
- `__init__()`: Create empty series
- `__init__(intervals: List[Interval])`: Create from list of Interval objects
- `__init__(intervals: List[Tuple[float, float]])`: Create from list of (start, end) tuples
- `add_event(interval: Interval)`: Add an interval to the series
- `get_intervals() -> List[Interval]`: Get all intervals
- `is_event_at_time(time: TimeFrameIndex) -> bool`: Check if event exists at time
- `size() -> int`: Number of intervals (also accessible via `len()`)
- `remove_interval(interval: Interval) -> bool`: Remove a specific interval
- `remove_intervals(intervals: List[Interval]) -> int`: Remove multiple intervals
- `create_intervals_from_bool(bool_vector: List[bool])`: Create intervals from boolean vector

### BooleanParams
Parameters for boolean operations between interval series.

**Properties:**
- `operation` (BooleanOperation): The operation to perform
- `other_series` (DigitalIntervalSeries): Second series for binary operations

### BooleanOperation (Enum)
Types of boolean operations.

**Values:**
- `AND`: Intersection of intervals (both must be true)
- `OR`: Union of intervals (either can be true)
- `XOR`: Exclusive or (exactly one must be true)
- `NOT`: Invert the input series
- `AND_NOT`: Subtract other from input

### PythonTransform
Create custom transforms implemented in Python.

**Constructor:**
- `__init__(name: str, callable: Callable)`: Create a transform with a name and callable function

The callable must take a DigitalIntervalSeries and return a DigitalIntervalSeries.

## Testing

Run the test suite:

```bash
cd tests/PythonBindings
python3 test_bindings.py
```

## Use Cases

1. **Prototyping Data Analysis**: Quickly test analysis algorithms in Python before implementing in C++
2. **Custom Filtering**: Create domain-specific filters for interval data
3. **Interactive Analysis**: Use Python notebooks for exploratory data analysis
4. **Pipeline Composition**: Combine C++ transforms with Python transforms in a processing pipeline
5. **Visualization**: Export data to Python for visualization with matplotlib, plotly, etc.

## Performance Considerations

- Python transforms have overhead from crossing the Python/C++ boundary
- For production use on large datasets, consider implementing transforms in C++
- Use Python transforms for prototyping and domain-specific logic
- The underlying data structures remain in C++ memory for efficiency
