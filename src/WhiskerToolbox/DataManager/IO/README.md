# Plugin System Integration with CoreGeometry

## Architecture Overview

The plugin system now uses the shared CoreGeometry library for all geometric types:

```cpp
// CoreGeometry Library (Shared)
├── Point2D<T>     // Template point class
├── Line2D         // Vector of Point2D<float>
└── ImageSize      // Width/height container

// DataManagerIO (Plugin Interfaces)
├── DataFactory    // Uses CoreGeometry types
├── LoadedDataVariant
└── LineDataRaw    // Raw data from file formats

// DataManagerIO_CapnProto (Plugin)
├── CapnProtoLoader    // Extracts raw data
└── No geometry deps   // Uses factory for object creation

// DataManager (Core)
├── ConcreteDataFactory  // Creates objects using CoreGeometry
└── LineData            // Uses CoreGeometry types internally
```

## Benefits

✅ **Consistent Types**: All plugins use the same Point2D/Line2D definitions  
✅ **Shared Implementation**: No duplication of geometric algorithms  
✅ **Optimized**: CoreGeometry is compiled once, shared across plugins  
✅ **Extensible**: New geometry types automatically available to all plugins  

## Usage Example

```cpp
// In CapnProto plugin - extracts raw coordinates
LineDataRaw raw_data;
raw_data.time_lines[time].push_back({{x1, y1}, {x2, y2}});

// In ConcreteDataFactory - creates proper objects
Line2D line;
line.push_back(Point2D<float>{x1, y1});
line.push_back(Point2D<float>{x2, y2});
auto line_data = std::make_shared<LineData>(data_map);
```

This ensures type consistency across the entire plugin ecosystem while maintaining clean architectural boundaries.
