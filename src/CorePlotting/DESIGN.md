# **CorePlotting Library Design Document**

## **1\. Overview**

CorePlotting is the foundational, Qt-independent C++ library for the Neuralyzer plotting stack. It serves as the mathematical and logical backbone for data visualization, providing unified abstractions that enforce a strict separation between **data & layout** (the "what" and "where") from **rendering** (the "how") and **interaction** (the "action").

This library is part of a strictly layered architecture designed to maximize testability and portability:

1. **CorePlotting** (This Library): The "brain" of the operation. It houses data structures, Layout engines, Coordinate transforms, Interaction state machines, and Spatial Indexing structures like QuadTrees. It is strictly **Qt-independent and OpenGL-agnostic**, ensuring that complex layout logic and mathematical transformations can be unit-tested in isolation without requiring a windowing system or a graphics context.  
2. **PlottingOpenGL**: The rendering backend. This library acts as a consumer of CorePlotting's scene descriptions. It translates abstract batches of geometry into efficient OpenGL draw calls, managing VBOs, VAOs, and shaders. It isolates all hardware-accelerated graphics API calls, allowing the rendering strategy to evolve independently of the logic.  
3. **Specific Qt Widgets**: The integration layer. These widgets (e.g., OpenGLWidget) handle OS-level input events (mouse clicks, key presses), manage the GL context lifecycle, and act as the bridge that translates raw user events into CorePlotting interaction queries and layout requests.

## **2\. Architecture High-Level**

The architecture follows a strict **unidirectional data flow** model to prevent state synchronization issues:

Data Source ──▶ Layout Engine ──▶ Mapper ──▶ Scene Builder ──▶ Renderable Scene ──▶ Renderer

### **The Three-Layer Model**

This model decouples the visualization semantics from the generic geometry required for drawing.

┌─────────────────────────────────────────────────────────────────────────────┐  
│                       Visualization-Specific Layer                          │  
│  ┌──────────────────┐  ┌──────────────────┐  ┌──────────────────────────┐   │  
│  │ TimeSeriesMapper │  │  SpatialMapper   │  │      RasterMapper        │   │  
│  │ (DataViewer)     │  │ (SpatialOverlay) │  │      (PSTH/Raster)       │   │  
│  └────────┬─────────┘  └────────┬─────────┘  └────────────┬─────────────┘   │  
│           │                     │                         │                 │  
│           ▼                     ▼                         ▼                 │  
│      positions\[\]           positions\[\]               positions\[\]            │  
│      entity\_ids\[\]          entity\_ids\[\]              entity\_ids\[\]           │  
└─────────────────────────────────────────────────────────────────────────────┘  
                                    │  
                                    ▼  
┌─────────────────────────────────────────────────────────────────────────────┐  
│                        CorePlotting (Generic Layer)                         │  
│                                                                             │  
│  ┌─────────────────────────────────────────────────────────────────────┐    │  
│  │                            SceneBuilder                             │    │  
│  │  • addGlyphs(key, positions\[\], entity\_ids\[\])                        │    │  
│  │  • addRectangles(key, bounds\[\], entity\_ids\[\])                       │    │  
│  │  • addPolyLines(key, lines\[\], entity\_ids\[\])                         │    │  
│  │  • setBounds() / setViewState() / build()                           │    │  
│  │                                                                     │    │  
│  │  Does NOT know about TimeFrame, specific data types, or extractors. │    │  
│  │  It only knows about abstract geometry (points, lines, quads).      │    │  
│  └─────────────────────────────────────────────────────────────────────┘    │  
│                                    │                                        │  
│                                    ▼                                        │  
│  ┌─────────────────────────────────────────────────────────────────────┐    │  
│  │                           RenderableScene                           │    │  
│  │  • glyph\_batches\[\], rectangle\_batches\[\], polyline\_batches\[\]         │    │  
│  │  • spatial\_index (QuadTree\<EntityId\>)                               │    │  
│  │  • view\_matrix, projection\_matrix                                   │    │  
│  └─────────────────────────────────────────────────────────────────────┘    │  
└─────────────────────────────────────────────────────────────────────────────┘

1. **Visualization-Specific Layer**: Mappers exist here. They understand domain-specific concepts like "TimeFrames," "Event Series," or "PSTH Trials" and convert them into raw coordinates.  
2. **Generic Layer**: The SceneBuilder exists here. It treats all inputs as generic geometric primitives (points, lines, rectangles) associated with Entity IDs, completely ignorant of the source data structure.  
3. **Scene Output**: The RenderableScene is a self-contained snapshot of the frame, ready for rendering or hit-testing.

## **3\. Layout Engine**

The Layout Engine determines **where** data series are positioned in the world space. It provides a clean separation between data semantics (the values) and spatial arrangement (the screen position).

### **Core Abstraction: LayoutTransform**

At the heart of the layout system is the LayoutTransform structure—a lightweight representation of a linear affine transform defined by a single scaling factor (gain) and a translation (offset):

output \= input \* gain \+ offset

This deceptively simple abstraction is the fundamental building block for all vertical positioning. By composing multiple transforms, we can handle complex visualization requirements without modifying the underlying data:

* **Data normalization**: Maps raw, potentially large or small data values (e.g., voltage in microvolts) to a standardized normalized range (typically \-1.0 to \+1.0).  
* **Vertical stacking**: Applies a translation offset to position specific series within their allocated slice of the viewport, ensuring they do not overlap unless desired.  
* **User adjustments**: Applies runtime modifications such as manual vertical scaling or offsetting, allowing users to "zoom in" on a specific trace amplitude without changing the global camera zoom.

### **Normalization Helpers**

Data normalization (z-score, peak-to-peak, etc.) is **widget-specific**, as different visualizations require different statistical interpretations. CorePlotting provides factory functions in NormalizationHelpers to create transforms from data statistics, bridging the gap between raw data stats and layout geometry:

// 1\. Compute stats (in widget logic)  
float mean \= series.mean;  
float std\_dev \= series.std\_dev;

// 2\. Create normalization transform (data → normalized \[-1, 1\])  
//    This logic lives in CorePlotting but is invoked by the widget.  
auto normalize \= NormalizationHelpers::forStdDevRange(mean, std\_dev, 3.0f);

// 3\. Get layout transform (normalized → world position)  
//    This comes from the LayoutEngine's response.  
auto layout \= layout\_response.findLayout(key).y\_transform;

// 4\. Compose: (data → normalized) → positioned  
//    The result is a single lightweight struct representing the total transform.  
auto final\_transform \= layout.compose(normalize);

### **Layout Strategies**

The LayoutEngine uses pluggable strategies (Strategy Pattern) to determine positioning rules. This allows the application to switch between different plot types dynamically:

1. **StackedLayoutStrategy** (DataViewer):  
   * **Stackable** series (Analog, Event): Divides the viewport height equally among active series. Used for standard electrophysiology plots where channels are stacked.  
   * **Full-canvas** series (Intervals): Assigned a transform that spans the entire viewport height, useful for background shading or epochs that apply to all channels.  
2. **RowLayoutStrategy** (Raster Plot):  
   * Treats every series as a discrete row.  
   * Rows have equal height and are ordered top-to-bottom.  
   * Essential for PSTH or trial-based visualizations where vertical alignment is strictly categorical.  
3. **SpatialLayoutStrategy** (SpatialOverlay):  
   * Differs from the others by transforming **both** X and Y coordinates.  
   * **Fit Mode**: Uniform scaling to fit data bounds within the viewport while preserving aspect ratio.  
   * **Fill Mode**: Non-uniform scaling to fill the viewport completely.  
   * **Identity**: Uses raw coordinates directly (1:1 mapping).

## **4\. Coordinate Mapping (Mappers)**

Mappers are the bridge between **Data** (domain objects like AnalogTimeSeries) and **Geometry** (points/lines). They apply the Layout Transforms and convert domain coordinates (like Time) into World coordinates (Floating point X/Y).

### **Range-Based Design**

A critical performance feature of the Mapper layer is its reliance on **C++20 ranges**. Mappers do not allocate intermediate containers (like std::vector) to hold transformed points. Instead, they return lightweight view objects that generate coordinates lazily upon iteration. This design provides three major benefits:

1. **Zero-copy Efficiency**: Data flows directly from the source DataManager structures into the destination GPU buffers and QuadTrees. There are no heap allocations for temporary storage, significantly reducing memory bandwidth pressure during frame updates.  
2. **Single Traversal**: The X coordinate (time), Y coordinate (value), and EntityId are extracted and computed in a single pass. The SceneBuilder consumes this stream once, populating both the rendering geometry and the spatial index simultaneously, ensuring they are perfectly synchronized.  
3. **Composability**: Mappers can be easily chained with standard range adaptors. For example, a mapper's output can be piped into std::views::filter to remove off-screen points or std::views::transform to apply additional visual effects, all without modifying the mapper logic itself.

### **Available Mappers**

#### **TimeSeriesMapper**

Designed for DataViewer-style plots where the X-axis represents Time and the Y-axis represents Value or Row Index.

* mapEvents(...) → Range of MappedElement (Discrete markers).  
* mapIntervals(...) → Range of MappedRectElement (Time-spanning regions).  
* mapAnalogSeries(...) → Range of MappedVertex (Continuous poly-line points).

#### **SpatialMapper**

Designed for plots where X and Y coordinates are inherent to the data (Points, Lines, Contours).

* mapPointsAtTime(...) → Range of MappedElement.  
* mapLinesAtTime(...) → Range of MappedLineView (Iterates over collections of lines).

#### **RasterMapper**

Designed for PSTH/Raster plots where the X-axis represents **relative time** (e.g., Time since stimulus onset).

* mapEventsRelative(...) → Range of MappedElement where X is calculated as Event Time \- Reference Time.

## **5\. Scene Graph**

The RenderableScene is a pure data description of exactly what should be drawn on the screen. It contains **Batched Primitives** specifically designed for efficient OpenGL rendering (using Instancing or MultiDraw commands).

### **SceneBuilder**

The SceneBuilder provides a fluent API to construct the scene incrementally. Crucially, it automatically handles the synchronization between **Rendering Geometry** and **Spatial Indexing**. By adding an element to the builder, it is guaranteed to be both visible (added to the batch) and interactable (added to the QuadTree).

RenderableScene scene \= SceneBuilder()  
    .setBounds(data\_bounds)           // Defines QuadTree area  
    .setViewState(view\_state)         // Sets View/Proj matrices  
    .addGlyphs("events",   
        TimeSeriesMapper::mapEvents(events, layout, tf))  
    .addRectangles("intervals",   
        TimeSeriesMapper::mapIntervals(intervals, layout, tf))  
    .build();

### **Components**

* **RenderablePolyLineBatch**: Contains flattened vertices (x, y, x, y...) for analog signals or lines. Optimized for glMultiDrawArrays.  
* **RenderableGlyphBatch**: Contains instance positions and colors for point markers (Events). Optimized for Instanced Rendering.  
* **RenderableRectangleBatch**: Contains bounds (x, y, w, h) and colors for quads (Intervals). Optimized for Instanced Rendering.  
* **QuadTree\<EntityId\>**: A spatial index for efficient hit testing. It is **owned by the scene** to guarantee that the spatial lookup structure always matches the currently rendered geometry frame.

## **6\. Coordinate Systems & Transforms**

### **The World Space Strategy**

To support high-performance visualization (100k+ points) with smooth, responsive zooming, the architecture employs a **World Space Strategy**:

1. **Static Buffers**: Rendering geometry is generated in **World Space** (Time units for X, Layout units for Y) and uploaded to the GPU once. It is **not** regenerated during simple camera operations like panning or zooming.  
2. **GPU Transformation**: Zoom and Pan operations are handled entirely by the Vertex Shader via uniform View/Projection matrices. This allows 60fps interaction even with massive datasets.  
3. **Inverse Querying**: Since the CPU-side spatial index (QuadTree) is also in World Space, mouse events are transformed from Screen Coordinates → World Coordinates to query the index.

### **ViewState Models**

1. **ViewState** (Spatial):  
   * Standard 2D camera model (Zoom X/Y, Pan X/Y).  
   * Used for SpatialOverlay where the user navigates a 2D plane.  
2. **TimeSeriesViewState** (Temporal):  
   * **X-Axis (Time Window)**: Defined by start\_time and end\_time. Changing this window triggers a **buffer rebuild**, as it implies a change in the Level of Detail (LOD) or streaming new data chunks.  
   * **Y-Axis (Visual)**: Handles standard zoom/pan via MVP matrices only. Changing vertical zoom does *not* require data regeneration.

### **Inverse Transforms**

Utilities located in CoordinateTransform/InverseTransform.hpp facilitate the interaction loop.

Canvas (Pixels) ──\> NDC ──\> World ──\> Data (TimeIndex, Value)

These functions reverse the rendering pipeline, converting a pixel click back into a precise TimeFrameIndex or data value.

## **7\. Interaction System**

The Interaction module provides a unified, reusable system for creating and modifying visual elements (Intervals, Lines, Rectangles) across different widgets.

### **Architecture**

┌──────────────────┐    ┌─────────────────────────────┐    ┌─────────────────┐  
│   OpenGLWidget   │───▶│ IGlyphInteractionController │───▶│  GlyphPreview   │  
│ (Mouse Events)   │    │ (State Machine)             │    │ (Canvas Coords) │  
└──────────────────┘    └─────────────────────────────┘    └────────┬────────┘  
         │                                                          │  
         ▼                                                          ▼  
┌──────────────────┐    ┌─────────────────────────────┐    ┌─────────────────┐  
│  SceneHitTester  │    │       RenderableScene       │◀───│ PreviewRenderer │  
│ (Finds Target)   │    │ (previewToDataCoords)       │    │ (Draws Overlay) │  
└──────────────────┘    └─────────────────────────────┘    └─────────────────┘

### **Controllers**

Controllers work explicitly in **Canvas Coordinates** (Pixels). This decision avoids the complexity of dealing with constantly changing View matrices during interactive operations (like dragging), where the camera might move simultaneously.

* **RectangleInteractionController**:  
  * **Creation**: Handles click-and-drag to create new intervals or boxes.  
  * **Modification**: Supports dragging specific edges (left/right/top/bottom) to resize existing elements.  
  * **Modes**: Can be constrained to "Interval Mode" (height always spans viewport) or "Box Mode" (free resizing).  
* **LineInteractionController**:  
  * **Creation**: Handles click-and-drag to draw lines.  
  * **Modification**: Supports dragging endpoints to reorient lines.  
  * **Snapping**: Includes logic to snap lines to horizontal/vertical axes during creation.

### **Data Flow for Interaction**

1. **Start**: The Widget detects a mouse click. It uses SceneHitTester to check for objects.  
   * If an object is hit: Initiates **Modification Mode**, passing the EntityId and original geometry to the controller.  
   * If empty space: Initiates **Creation Mode**.  
2. **Update**: On mouse move, the Controller updates its internal state and produces a GlyphPreview.  
3. **Render**: The PreviewRenderer draws the GlyphPreview (often semi-transparent "ghosts" or active shapes) on top of the main scene.  
4. **Commit**: On mouse release, the Widget calls scene.previewToDataCoords(preview). This uses the inverse transforms to convert the pixel-based preview back into DataCoordinates (e.g., TimeFrameIndex start/end) which are then committed to the DataManager.

## **8\. Hit Testing**

Hit testing answers the question: "What did the user click on?".

The SceneHitTester orchestrates multiple strategies based on the nature of the data:

1. **QuadTree**: Performs an O(log n) spatial search for discrete elements (Events, Points). It returns the specific EntityId of the hit item.  
2. **Interval Query**: Checks strictly for time-based containment (X-axis) for intervals. Returns EntityId.  
3. **Series Region**: Checks if the cursor Y-coordinate falls within a series' allocated layout height. This returns a series\_key (no EntityId), useful for selecting the "active trace."

Priority Rule: Discrete \> Interval Edge \> Interval Body \> Series Region.  
(e.g., Clicking a specific Event marker takes precedence over clicking the general background of the Trace it sits on).

## **9\. Directory Structure**

src/CorePlotting/  
├── CoordinateTransform/       \# Matrices, ViewState, TimeRange, Inverse utilities  
├── DataTypes/                 \# SeriesStyle, SeriesConfig, and shared data types  
├── Export/                    \# Logic for exporting scenes to SVG format  
├── Interaction/               \# Controllers, HitTesting logic, and Preview structs  
├── Layout/                    \# LayoutEngine, LayoutStrategies, and Transform logic  
├── Mappers/                   \# Range-based Data \-\> Geometry mappers  
├── SceneGraph/                \# RenderableScene, SceneBuilder, and Primitive definitions  
├── SpatialIndex/              \# QuadTree implementation  
└── Transformers/              \# Data processing utilities (e.g., GapDetector)  
