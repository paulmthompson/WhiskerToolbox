# Display Options for Media Widgets

## Overview

The display system in WhiskerToolbox allows users to customize the visual appearance of various data types including points and lines through configurable display options.

## Point Display Options

### Point Size Control
- **Range**: 1-50 pixels
- **UI Controls**: 
  - Horizontal slider for quick adjustments
  - Spin box for precise numeric input
  - Synchronized controls (changing one updates the other)
- **Default Value**: 5 pixels

### Marker Shape Options
The system supports six different marker shapes:

1. **Circle** (default) - Filled circular marker
2. **Square** - Filled rectangular marker  
3. **Triangle** - Filled triangular marker pointing upward
4. **Cross** - Plus sign (+) shaped marker
5. **X** - X-shaped marker (×)
6. **Diamond** - Diamond shaped marker (rotated square)

### Implementation Details

- Point configuration is stored in the `PointDisplayOptions` structure
- UI controls are synchronized to prevent conflicts
- Real-time updates are supported via Qt signals/slots
- Different marker shapes are rendered using appropriate Qt drawing primitives

## Line Display Options

### Line Thickness Control
- **Range**: 1-20 pixels
- **UI Controls**:
  - Horizontal slider for quick adjustments
  - Spin box for precise numeric input
  - Synchronized controls (changing one updates the other)
- **Default Value**: 2 pixels

### Additional Line Features
- **Show Points**: Option to display open circles at each data point along the line
- **Edge Snapping**: Enable automatic snapping to detected edges when adding points
- **Color and Alpha**: Configurable line color and transparency

### Implementation Details

- Line configuration is stored in the `LineDisplayOptions` structure  
- Line thickness is applied via `QPen::setWidth()` during rendering
- UI controls follow the same synchronization pattern as point controls
- Real-time updates are supported via Qt signals/slots

## Technical Architecture

### Display Options Structure
```cpp
struct PointDisplayOptions : public BaseDisplayOptions {
    int point_size{DefaultDisplayValues::POINT_SIZE};
    PointMarkerShape marker_shape{DefaultDisplayValues::POINT_MARKER_SHAPE};
};

struct LineDisplayOptions : public BaseDisplayOptions {
    int line_thickness{DefaultDisplayValues::LINE_THICKNESS};
    bool show_points{DefaultDisplayValues::SHOW_POINTS};
    bool edge_snapping{false};
};
```

### Widget Integration
- MediaPoint_Widget handles point-specific controls
- MediaLine_Widget handles line-specific controls
- Media_Window applies the configurations during rendering
- Synchronized UI controls prevent user confusion

### Rendering System
The rendering system in Media_Window applies the display options during the plotting phase:
- Point markers are drawn using the configured size and shape
- Lines are drawn using the configured thickness via QPen
- All configurations support real-time updates without data loss

## Usage Guidelines

### For Users
1. Select the data item from the feature table
2. Adjust display options in the widget panel
3. Changes are applied immediately to the visualization
4. Use sliders for quick adjustments or spinboxes for precise values

### For Developers
1. All display options inherit from `BaseDisplayOptions`
2. UI controls should use blockSignals() to prevent recursive updates
3. Follow the established naming convention for slot functions
4. Add corresponding test cases for new display options
5. Document new features in this file 