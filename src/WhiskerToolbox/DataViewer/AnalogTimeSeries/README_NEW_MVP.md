# New MVP Matrix System for Analog Time Series

## Overview

This document describes the new Model-View-Projection (MVP) matrix system implemented for analog time series visualization in the WhiskerToolbox DataViewer. This system is designed to replace the existing MVP infrastructure with a more robust, testable, and feature-rich implementation.

## Key Features

### 1. Three-Tier Scaling System
- **Intrinsic Scaling**: Data-based normalization (currently 3 standard deviations)
- **User-Specified Scaling**: User-controlled amplitude and positioning adjustments
- **Global Scaling**: System-wide zoom and scale factors

### 2. PlottingManager Class
- Coordinates multiple analog and digital series
- Manages viewport allocation and coordinate assignment
- Replaces the existing VerticalSpaceManager for improved functionality
- Handles global transformations and panning

### 3. Comprehensive Testing
- Unit tests with Catch2 framework
- 100,000-point Gaussian data generation for realistic testing
- Gold standard validation for coordinate transformations
- Edge case handling for zero standard deviation and extreme values

## Data Structures

### AnalogScalingConfig
```cpp
struct AnalogScalingConfig {
    float intrinsic_scale{1.0f};        // Data-based normalization
    float intrinsic_offset{0.0f};       // Data-based vertical offset
    float user_scale_factor{1.0f};      // User amplitude control
    float user_vertical_offset{0.0f};   // User positioning control
    float global_zoom{1.0f};            // Global zoom factor
    float global_vertical_scale{1.0f};  // Global vertical scaling
};
```

### NewAnalogTimeSeriesDisplayOptions
- Comprehensive display configuration
- Includes scaling configuration, visual properties, and coordinate allocation
- Maintains compatibility with existing color and visibility systems

### PlottingManager
- Manages series registration and coordinate allocation
- Handles viewport configuration and data range management
- Provides methods for calculating series-specific positioning

## Function Implementation

### Model Matrix (`new_getAnalogModelMat`)
- Handles series-specific positioning and scaling
- Applies translation to allocated center position
- Implements three-tier scaling system with division-by-zero protection
- Uses 80% of allocated height for safe visual spacing

### View Matrix (`new_getAnalogViewMat`)
- Handles global view transformations
- Currently implements vertical panning offset
- Designed for future expansion with additional view-level effects

### Projection Matrix (`new_getAnalogProjectionMat`)
- Maps data coordinates to normalized device coordinates (NDC)
- Takes data index ranges instead of time for testing flexibility
- Applies viewport configuration and pan offsets

## Testing Framework

### Test Categories
1. **Happy Path Tests**: Normal operation with realistic data
2. **Edge Case Tests**: Zero standard deviation, extreme scaling values
3. **Error Handling**: Invalid ranges, large dataset indices

### Key Test Scenarios
- **Single Gaussian Series**: 100,000 points, mean=0, sigma=10
- **Multiple Series Allocation**: Coordinate distribution and non-overlap verification
- **Scaling Validation**: User and global scaling factor effects
- **Coordinate System Validation**: NDC range compliance

### Gold Standard Validation
- Data points 1-10000 mapped to X-axis range [-1, 1]
- ±3 standard deviations scaled by intrinsic normalization and 80% height factor
- +3σ maps to `allocated_center + (allocated_height * 0.8)`
- -3σ maps to `allocated_center - (allocated_height * 0.8)`
- Center values (data value = 0) mapped to allocated center coordinates
- Series separation maintained for multiple series

## Files Created/Modified

### New Files
- `MVP_AnalogTimeSeries.test.cpp`: Comprehensive test suite
- `MVP_AnalogTimeSeries_demo.cpp`: Interactive demonstration program
- `README_NEW_MVP.md`: This documentation file

### Modified Files
- `MVP_AnalogTimeSeries.hpp`: Added new data structures and function declarations
- `MVP_AnalogTimeSeries.cpp`: Added new function implementations
- `CMakeLists.txt`: Added test file and demo executable to build system

## Usage Example

```cpp
// Set up plotting manager
PlottingManager manager;
int series_idx = manager.addAnalogSeries();
manager.setVisibleDataRange(1, 10000);

// Configure display options
NewAnalogTimeSeriesDisplayOptions display_options;
float allocated_center, allocated_height;
manager.calculateAnalogSeriesAllocation(series_idx, allocated_center, allocated_height);
display_options.allocated_y_center = allocated_center;
display_options.allocated_height = allocated_height;

// Generate MVP matrices
glm::mat4 model = new_getAnalogModelMat(display_options, std_dev, manager);
glm::mat4 view = new_getAnalogViewMat(manager);
glm::mat4 projection = new_getAnalogProjectionMat(1, 10000, -1.0f, 1.0f, manager);

// Transform data points
glm::vec4 data_point(index, value, 0.0f, 1.0f);
glm::vec4 ndc_point = projection * view * model * data_point;
```

## Future Enhancements

### Planned Features
1. **Digital Series Support**: Extend PlottingManager for DigitalEvent and DigitalInterval series
2. **Advanced Scaling Options**: Peak-to-peak normalization, percentile-based scaling
3. **Interactive Features**: Selection, editing, and real-time scaling adjustments
4. **Performance Optimizations**: Caching, vectorized transformations
5. **Additional Projections**: Non-linear scaling, logarithmic axes

### Integration Path
1. Complete analog series implementation and testing
2. Extend to digital series types
3. Replace existing VerticalSpaceManager usage
4. Integrate with OpenGL rendering pipeline
5. Add user interface controls

## Testing and Validation

Run the comprehensive test suite:
```bash
# Build and run tests (implementation-dependent on your build system)
make test_dataviewer
./test_dataviewer "[mvp][analog][new]"
```

Run the interactive demo:
```bash
# Build and run demo
make mvp_analog_demo
./mvp_analog_demo
```

## Technical Notes

### Coordinate Systems
- **Data Coordinates**: Original data indices and values
- **World Coordinates**: After model transformation (centered, scaled)
- **View Coordinates**: After view transformation (global effects applied)  
- **Normalized Device Coordinates**: Final [-1, 1] range for OpenGL rendering

### Performance Considerations
- Division-by-zero protection for edge cases
- Efficient matrix operations using GLM library
- Minimal memory allocations in hot paths
- Cached standard deviation calculations

### Compatibility
- Maintains compatibility with existing GLM and OpenGL infrastructure
- Follows established project coding standards and documentation practices
- Designed for seamless integration with existing DataViewer components 