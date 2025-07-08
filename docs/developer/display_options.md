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
- **Position Marker**: Display a marker at a specified percentage distance along the line (off by default)
  - `show_position_marker` (bool): Enable/disable position marker display
  - `position_percentage` (int, 0-100%): Position along line where marker appears
- **Line Segment**: Display only a portion of the line between two percentage points (off by default)
  - `show_segment` (bool): Enable/disable segment-only display mode
  - `segment_start_percentage` (int, 0-100%): Start percentage for line segment
  - `segment_end_percentage` (int, 0-100%): End percentage for line segment
- **Color and Alpha**: Configurable line color and transparency

### Position Marker Details
- **Range**: 0-100% along the cumulative length of the line
- **UI Controls**:
  - Checkbox to enable/disable the feature (off by default)
  - Horizontal slider for quick adjustments
  - Spin box for precise numeric input with % suffix
- **Default Value**: 20% along the line
- **Visual Appearance**: Distinctive filled circle with white border, same color as the line
- **Calculation**: Based on cumulative distance along line segments, not point indices

### Implementation Details

- Line configuration is stored in the `LineDisplayOptions` structure  
- Line thickness is applied via `QPen::setWidth()` during rendering
- Position marker calculated using cumulative distance along line segments
- UI controls follow the same synchronization pattern as point controls
- Real-time updates are supported via Qt signals/slots

#### Line Segment Rendering

The segment feature allows displaying only a portion of the line between two percentage points along its cumulative distance:

- **`get_segment_between_percentages()`**: Utility function in `lines.hpp` that extracts a continuous segment
  - Calculates cumulative distances along the original line
  - Performs linear interpolation for precise start/end points
  - Returns a new `Line2D` containing only the specified segment
  - Handles edge cases (empty lines, invalid percentages, zero-length segments)

- **UI Validation**: Start percentage cannot exceed end percentage (enforced in UI)
- **Rendering Logic**: When enabled, replaces the full line with the extracted segment in all rendering operations
- **Position Marker Compatibility**: Position markers work correctly with segments (percentage relative to segment, not original line)

#### Mask Bounding Box Rendering

The bounding box feature provides a visual outline around mask regions:

- **`get_bounding_box()`**: Utility function in `masks.hpp` that calculates min/max coordinates
  - Iterates through all mask points to find extrema
  - Returns pair of Point2D representing opposite corners
  - Handles single-point masks correctly

- **Rendering Logic**: Draws unfilled rectangles using Qt's `addRect()` with `Qt::NoBrush` 
- **Coordinate Scaling**: Applies same aspect ratio scaling as mask data
- **Multiple Masks**: Each mask gets its own bounding box when feature is enabled
- **Time Handling**: Bounding boxes are drawn for both current time and time -1 masks
- **Container Management**: Uses separate `_mask_bounding_boxes` container (similar to digital intervals) to avoid type conflicts

### Utility Functions

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
    bool show_position_marker{false};
    int position_percentage{20};
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
- Position markers use `get_position_at_percentage()` for accurate placement
- All configurations support real-time updates without data loss

## Usage Guidelines

### For Users
1. Select the data item from the feature table
2. Adjust display options in the widget panel
3. Changes are applied immediately to the visualization
4. Use sliders for quick adjustments or spinboxes for precise values
5. Enable position marker to highlight specific locations along lines

### For Developers
1. All display options inherit from `BaseDisplayOptions`
2. UI controls should use blockSignals() to prevent recursive updates
3. Follow the established naming convention for slot functions
4. Add corresponding test cases for new display options
5. Use `get_position_at_percentage()` for accurate line position calculations
6. Document new features in this file

### Mask Display Options

Mask data represents 2D regions or shapes that can be overlaid on the media canvas:

- **Color and Alpha**: Configurable mask color and transparency
- **Bounding Box**: Display rectangular outline around the mask extent (off by default)
  - `show_bounding_box` (bool): Enable/disable bounding box display

#### Technical Implementation

### Mask Display Features

The mask display system includes:

- **Color and Alpha Control**: Masks can be displayed with configurable colors and transparency levels
- **Bounding Box**: Option to display a rectangular outline around each mask showing its bounds
- **Outline Drawing**: Option to display the mask boundary as a thick line by connecting extremal points

#### Bounding Box Feature

When enabled, a bounding box renders a rectangle outline around each mask. The implementation:
- Uses the existing `get_bounding_box()` utility function from `masks.hpp` 
- Scales coordinates properly with aspect ratios
- Draws unfilled rectangles using `Qt::NoBrush` for outline-only appearance
- Handles both current time and time -1 masks

#### Outline Feature

When enabled, displays the mask boundary as a thick line connecting extremal points. The algorithm:
- For each unique x coordinate, finds the maximum y value
- For each unique y coordinate, finds the maximum x value  
- Collects all extremal points and sorts them by angle from centroid
- Connects points to form a closed boundary outline
- Renders as a thick 4-pixel wide line using `QPainterPath`

The outline feature uses the `get_mask_outline()` function to compute boundary points by finding extremal coordinates, providing a visual representation of the mask's outer boundary.

### UI Integration