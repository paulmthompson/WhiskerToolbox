# V2 Line Minimum Point Distance Transform

## Overview

The `LineMinPointDistTransform` demonstrates V2's capability to handle transforms requiring multiple inputs by packaging them into a single composite structure.

## Architecture

### Input Structure

```cpp
struct LinePointPair {
    Line2D line;                         // The line to measure distance from
    std::vector<Point2D<float>> points;  // The points to measure distance to
};
```

This structure combines two data sources (lines and points) into a single input element, enabling the transform to work within the element-level framework.

### Transform Function

```cpp
float calculateLineMinPointDistance(
    LinePointPair const& input,
    LineMinPointDistParams const& params);
```

The transform:
1. Takes a line and set of points
2. Finds the minimum distance from any point to any segment of the line
3. Returns a single float value (distance or squared distance)

### Parameters

```cpp
struct LineMinPointDistParams {
    std::optional<bool> use_first_line_only;      // Default: true
    std::optional<bool> return_squared_distance;  // Default: false
};
```

## Usage in Pipeline

### Step 1: Prepare Data

Given `LineData` and `PointData` containers, iterate through time indices:

```cpp
for (auto time : time_indices) {
    auto lines = line_data->getAtTime(time);
    auto points = point_data->getAtTime(time);
    
    // Combine into single input
    LinePointPair input = createLinePointPair(lines, points);
    
    // Apply transform
    float distance = calculateLineMinPointDistance(input, params);
    
    // Store in output (e.g., RaggedAnalogTimeSeries)
    output->appendAtTime(time, {distance});
}
```

### Step 2: Register and Execute

The transform is registered in the ElementRegistry:

```cpp
RegisterTransform<LinePointPair, float, LineMinPointDistParams>(
    "CalculateLineMinPointDistance",
    calculateLineMinPointDistance,
    metadata);
```

### Step 3: JSON Configuration

Example pipeline JSON:

```json
{
  "steps": [
    {
      "step_id": "distance_calc",
      "transform_name": "CalculateLineMinPointDistance",
      "parameters": {
        "use_first_line_only": true,
        "return_squared_distance": false
      }
    }
  ]
}
```

## Key Features

### 1. Multi-Input Support

By packaging multiple inputs into a composite structure (`LinePointPair`), we maintain the element-level transform interface while supporting complex multi-source operations.

### 2. Geometric Computation

The transform performs proper computational geometry:
- Distance to line segments (not infinite lines)
- Projection clamping to segment endpoints
- Minimum over all segments in polyline

### 3. Performance Options

```cpp
params.return_squared_distance = true;  // Skip sqrt for better performance
```

### 4. Flexible Input

```cpp
params.use_first_line_only = true;   // Use first line only
params.use_first_line_only = false;  // Could combine all lines (future)
```

## Algorithm Details

### Point-to-Segment Distance

For a point P and line segment from A to B:

1. **Project P onto line AB**:
   ```
   t = dot(P-A, B-A) / ||B-A||²
   ```

2. **Clamp to segment**:
   ```
   t = clamp(t, 0, 1)
   ```

3. **Find closest point on segment**:
   ```
   C = A + t(B-A)
   ```

4. **Calculate distance**:
   ```
   distance = ||P-C||
   ```

### Line Distance

For a point and multi-segment line:
- Compute distance to each segment
- Return minimum

## Integration with V1 System

The V2 transform provides equivalent functionality to the V1 `LineMinPointDistOperation`, but with:

- ✅ Strongly typed parameters (reflect-cpp)
- ✅ JSON serialization/deserialization
- ✅ Element-level composability
- ✅ Zero per-element dispatch overhead (typed executors)
- ✅ Pipeline integration via `loadPipelineFromJson`

## Future Extensions

### Multiple Lines

Currently uses first line only. Could extend to:
- Minimum distance across all lines
- Distance to specific line index
- Distance vector to each line

### Output Options

Could add parameters for:
- Return closest point coordinates
- Return which segment is closest
- Return distances to all points (not just minimum)

### Image Space Handling

V1 includes scaling between different image coordinate systems. V2 could add:

```cpp
struct LineMinPointDistParams {
    std::optional<ImageSize> line_image_size;
    std::optional<ImageSize> point_image_size;
    // ... scaling logic ...
};
```

## Testing

Comprehensive tests cover:
- ✅ Basic distance calculation
- ✅ Point on line
- ✅ Point beyond segment endpoints
- ✅ Multi-segment lines
- ✅ Empty/invalid inputs
- ✅ Squared distance mode
- ✅ JSON serialization
- ✅ Parameter defaults

See `test_line_min_point_dist_v2.test.cpp` for details.
