Analysis Dashboard Infrastructure: Design Document
1. Overview
This document outlines the software design for the Analysis Dashboard, a flexible and interactive data visualization tool built using C++ and the Qt framework. The dashboard allows users to dynamically create, arrange, and configure various types of plots on a canvas-like interface.

The core design is a three-panel layout:

Toolbox Panel (Left): A list of available plot types that can be dragged onto the central scene.

Graphics Scene (Center): A large, scrollable canvas (QGraphicsScene) where instantiated plot widgets are placed, moved, resized, and interacted with.

Properties Panel (Right): A context-sensitive panel that displays and allows modification of the properties for a selected plot widget or for the global dashboard environment.

The architecture is designed to be modular and extensible, allowing new plot types and their corresponding properties to be easily integrated into the system.

2. High-Level Architecture
The dashboard's infrastructure is composed of several key C++ classes that manage the UI, user interactions, and data flow.

Analysis_Dashboard (QMainWindow): The top-level window and central coordinating class. It is responsible for initializing the main layout, creating the primary panels (Toolbox, Scene, Properties), and connecting the core signals and slots that drive the application's logic.

AnalysisDashboardScene (QGraphicsScene): The heart of the visualization area. This custom scene manages the lifecycle of all plot widgets. It handles drag-and-drop events from the toolbox to instantiate new plots, manages plot selection, and communicates events from individual plots up to the main dashboard.

AbstractPlotWidget (QGraphicsWidget): An abstract base class for all visual plots (e.g., Scatter Plot, Spatial Overlay Plot). It defines the common interface for all plots, including selection, movement, resizing, and data handling. It ensures that any plot type can be treated polymorphically by the AnalysisDashboardScene.

ToolboxPanel (QWidget): A panel that presents a list of available plot types to the user. It initiates the drag-and-drop operation, embedding the plot type information into the MIME data, which is then used by the AnalysisDashboardScene to instantiate the correct plot widget.

PropertiesPanel (QWidget): A panel that displays configuration options. It listens for plotSelected signals from the scene. When a plot is selected, the panel dynamically populates its UI with the appropriate properties widget (derived from AbstractPlotPropertiesWidget) for that specific plot type.

DataManager: A shared service responsible for holding and providing access to the application's data. It is injected into the scene, which then passes it down to newly created plot widgets.

System Interaction Diagram
+----------------+       (1) Drag Plot Type       +--------------------------+
|  ToolboxPanel  | ----------------------------> |  AnalysisDashboardScene  |
+----------------+                               +--------------------------+
        ^                                                 |
        |                                                 | (2) Create Plot, Add to Scene
        |                                                 |
        |                                                 v
+-----------------------+ <--------------------+----------------------+
|  Analysis_Dashboard   | (4) plotSelected signal |  AbstractPlotWidget  |
| (Coordinator)         | ----------------------> +----------------------+
+-----------------------+                      (3) User clicks to select
        |
        | (5) showPlotProperties
        v
+-----------------+
| PropertiesPanel |
+-----------------+

3. Component Breakdown
3.1. Analysis_Dashboard
This class acts as the application's orchestrator.

Responsibilities:

Constructs and owns the ToolboxPanel, PropertiesPanel, and the QGraphicsView which hosts the AnalysisDashboardScene.

Arranges the three main components within a QSplitter for a flexible layout.

Connects the plotSelected, plotAdded, and plotRemoved signals from the AnalysisDashboardScene to its own slots.

When a plot is selected, it instructs the PropertiesPanel to display the properties for that plot.

3.2. AnalysisDashboardScene
This is the most dynamic component, managing the interactive canvas.

Responsibilities:

Handles drag-and-drop operations with the application/x-plottype MIME type.

On a successful drop, it acts as a factory, using createPlotFromMimeData to instantiate the correct concrete AbstractPlotWidget subclass.

Adds the newly created plot widget to the scene and stores it in a QMap<QString, AbstractPlotWidget*> for quick lookup by its unique ID.

Manages the selection state of the plots, ensuring only one can be active at a time.

Forwards signals from individual plots (e.g., plotSelected) to the main Analysis_Dashboard.

Provides the DataManager instance to each plot widget it creates.

3.3. AbstractPlotWidget
This base class ensures consistency and provides foundational features for all plots.

Responsibilities:

Generates a unique ID (plot_id) upon construction.

Implements standard QGraphicsWidget behavior for moving, resizing, and selection.

Defines a pure virtual getPlotType() method, forcing subclasses to declare their type.

Emits a plotSelected(plot_id) signal on a mouse press event.

Holds a shared pointer to the DataManager to access data.

Defines the interface for setting and getting a plot title.

4. Data Management and Flow
The data flow is decoupled from the UI components via an observer pattern centered on the DataManager.

Data Provider: The DataManager class serves as the single source of truth for all visualization data.

Observer Pattern: The DataManager and its internal data structures implement an observer pattern. They allow interested components (observers) to register callbacks (std::function) which are invoked upon specific data events (e.g., data added, removed, or changed).

Widget Subscription: When an AbstractPlotWidget is created and receives the DataManager instance, it becomes an observer. It subscribes to the relevant data sources within the DataManager by providing a callback function.

Data Updates: When data changes in the DataManager, it triggers the registered callbacks. The plot widget's callback function is executed, allowing the widget to fetch the new data and request a re-render by calling its update() method or emitting the renderUpdateRequested signal. This ensures that plots are always displaying the most current data without being tightly coupled to the data-loading logic.

5. Event Handling
5.1. Creating a New Plot
Drag Start: User clicks and drags an item from the ToolboxPanel. The panel initiates a QDrag operation with QMimeData of type application/x-plottype, containing the name of the plot (e.g., "scatter_plot").

Drag Enter/Move: The AnalysisDashboardScene accepts the drag event because it recognizes the MIME type.

Drop: The user releases the mouse over the scene. The dropEvent is triggered.

Instantiation: The scene's createPlotFromMimeData function reads the plot type from the MIME data and instantiates the corresponding concrete AbstractPlotWidget (e.g., new ScatterPlotWidget()).

Initialization: The scene adds the new widget to itself, passes the DataManager to it, and connects its signals.

Notification: The scene emits plotAdded(plot_id), notifying the main dashboard.

5.2. Selecting a Plot and Displaying Properties
Mouse Press: User clicks on a plot widget in the scene.

Signal Emission: The AbstractPlotWidget::mousePressEvent fires, emitting the plotSelected(plot_id) signal.

Scene Handling: The AnalysisDashboardScene catches this signal. It updates its internal selection state and re-emits its own plotSelected(plot_id) signal.

Dashboard Coordination: The Analysis_Dashboard's handlePlotSelected slot is invoked.

Properties Update: The dashboard calls _properties_panel->showPlotProperties(), passing the plot_id and a pointer to the selected widget.

UI Population: The PropertiesPanel clears its current content and populates itself with the specific properties UI corresponding to the selected plot's type.

6. Concrete Implementation Example: SpatialOverlayPlotWidget
The SpatialOverlayPlotWidget is a complex widget that demonstrates a robust implementation pattern for high-performance, interactive visualizations within the dashboard framework. It is designed to render large numbers of spatial data points and masks from multiple time series sources efficiently using OpenGL.

6.1. Architecture and Design Patterns
The widget's architecture is defined by a clear separation of concerns, achieved through composition and the use of proxy widgets.

Composition over Inheritance: The SpatialOverlayPlotWidget (a QGraphicsWidget) does not perform rendering itself. Instead, it contains and manages a SpatialOverlayOpenGLWidget (a QOpenGLWidget). This is the most critical design decision.

Widget Proxying: To embed the QOpenGLWidget within the QGraphicsScene, a QGraphicsProxyWidget is used. This allows the OpenGL rendering context to exist and operate within the item-based Graphics View Framework.

Controller-Renderer Split:

SpatialOverlayPlotWidget acts as the Controller. It handles its own geometry within the scene (resizing, moving), communicates with the PropertiesPanel, and manages which data sources are active. It delegates all rendering and low-level interaction to its child OpenGL widget.

SpatialOverlayOpenGLWidget acts as the Renderer. It manages the OpenGL context, shaders, VBOs, and all direct user interactions like mouse-based panning, zooming, and point/polygon selection.

Data Abstraction: The renderer uses dedicated visualization classes (PointDataVisualization, MaskDataVisualization) to encapsulate the OpenGL resources and spatial index (QuadTree, R-Tree) for each distinct data source. This makes it easy to manage multiple overlays.

6.2. Component Interaction Diagram
+------------------------------------+
| SpatialOverlayPlotPropertiesWidget |
| (Properties Panel UI)              |
+------------------------------------+
       |   ^
(1) User changes      | (5) Plot signals
    properties        |     property changes
       v   |
+------------------------------------+      (2) setPointDataKeys(), etc.
|      SpatialOverlayPlotWidget      | ----------------------------------> +----------------------------------+
|      (QGraphicsWidget Controller)  |                                     |    SpatialOverlayOpenGLWidget    |
+------------------------------------+ <---------------------------------- + (QOpenGLWidget Renderer/Interactor) |
       ^   |                                (4) frameJumpRequested, etc.   +----------------------------------+
       |   | (3) Creates/Manages                                                 |            ^
       |   |                                                                     |            | (6) Renders/Interacts with
       |   v                                                                     v            |
+--------------------------+                                            +--------------------+--------------------+
| QGraphicsProxyWidget     |                                            | PointDataVisualization | MaskDataVisualization |
+--------------------------+                                            +--------------------+--------------------+
                                                                              (Manages VBOs, VAOs, QuadTree/R-Tree)

6.3. Rendering and Interaction Pipeline
Configuration: The user selects data sources (e.g., PointData, MaskData) in the SpatialOverlayPlotPropertiesWidget.

Data Request: The properties widget calls a method on the SpatialOverlayPlotWidget (e.g., setPointDataKeys()).

Delegation: The SpatialOverlayPlotWidget forwards this request to the SpatialOverlayOpenGLWidget.

Data Processing: The SpatialOverlayOpenGLWidget fetches the requested data from the DataManager. For each data source, it creates a corresponding visualization object (PointDataVisualization, etc.). This object:

Builds a spatial index (QuadTree for points, R-Tree for masks) for fast queries.

Copies vertex data into a QOpenGLBuffer (VBO) and sets up a QOpenGLVertexArrayObject (VAO).

Rendering: In its paintGL method, the SpatialOverlayOpenGLWidget iterates through its visualization objects, binds their respective shaders and VAOs, and issues draw calls. It uses modern OpenGL shaders for rendering points, lines, and textures (for masks).

Interaction:

Mouse events (press, move, wheel) are handled directly by SpatialOverlayOpenGLWidget.

Pan/Zoom: Events modify the view and projection matrices, which are passed as uniforms to the shaders.

Selection/Hover: The mouse position is converted to world coordinates. The spatial index of each visualization object is queried to efficiently find the point/mask under the cursor, avoiding a linear scan of all data.

Feedback: If an interaction requires dashboard-level changes (e.g., a double-click requesting a time frame jump), the SpatialOverlayOpenGLWidget emits a signal (frameJumpRequested), which is caught by the SpatialOverlayPlotWidget and re-emitted to be handled by the main Analysis_Dashboard.

This concrete example demonstrates how the abstract infrastructure supports complex, high-performance widgets by providing a clear framework for coordination while allowing the specific implementation to choose the best technology (like raw OpenGL) for its task.