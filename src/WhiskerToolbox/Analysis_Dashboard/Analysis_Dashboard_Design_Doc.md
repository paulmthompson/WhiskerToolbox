# Analysis Dashboard
The Analysis Dashboard is the main widget.

Constructor: It takes the data manager and a time scroll bar as input arguments.

Ownership: It creates and owns the following components:

The Toolbox Panel (left)

The Properties Panel (right)

The Analysis Dashboard Scene (middle)

A Graphics View for the scene (middle)

Responsibilities:

It passes the data manager to its sub-widgets.

It connects signals and slots between the various components.

It performs widget management, getting alerted when a plot is added or removed. If the selected plot is removed, it tells the Properties Panel to revert to showing only global properties.

It holds the CreatePlotWidget factory method, which returns a pointer to an AbstractPlotWidget of a specified type.

As the owner of the time scroll bar, it resets a jump frame when a widget signals to do so.

It holds the unique pointer to the group manager and passes a raw pointer of it to other sub-widgets.

# Toolbox Panel
The Toolbox Panel is located on the left side of the dashboard.

Constructor: It is constructed with a raw pointer to the group manager and receives a shared pointer to the data manager.

UI Components: It is responsible for displaying:

The group manager interface.

The table view creation interface.

A list of available plot types that can be added to the graphics scene.

Ownership: It creates and owns the GroupManagerWidget. The TableDesignerWidget is now a top-level module in `WhiskerToolbox` and is launched from the Main Window.

Functionality: It contains UI components for listing plot types. When a button is clicked to add a plot, it sends a signal to the Analysis Dashboard to call the CreatePlotWidget factory method.

# Groups Management
Group Manager: The manager itself is a QObject to enable signal and slot functionality. It sends signals to the GroupsManagementWidget.

GroupsManagementWidget: This widget serves as an interface to the group manager.

It is constructed with a raw pointer to the group manager.

It uses numerous slots that are activated to display changes when groups are modified.

# Table Designer and Manager
Table Manager: This follows a similar pattern to the group manager.

It is a QObject.

It is constructed with a shared pointer to the DataManager.

It creates and stores a ComputeRegistry as a unique pointer.

It is stored as a unique pointer within the Toolbox Panel.

It holds the different table views that are constructed in a map.

Table Designer Widget: This is a Qt widget that provides the interface for the Table Manager.

# Analysis Dashboard Scene
The Analysis Dashboard Scene is a subtype of QGraphicsScene and acts as the canvas where all plot widgets are displayed.

Data Access: It holds pointers to the GroupManager and TableManager.

Ownership: It is the main owner of the plot widgets that are created, storing them in a map.

Responsibilities:

It moves the DataManager, GroupManager, and TableManager to the plot widgets.

It connects callbacks (signals and slots) to be alerted to events from the plots.

It handles QGraphicsScene-related tasks for ensuring plots are displayed correctly.

# Properties Panel
The Properties Panel is located on the right side of the dashboard.

Contents:

It holds a map of AbstractPlotPropertiesWidgets, with a one-to-one relationship to the plot widgets in the scene.

It stores a pointer to the currently selected plot widget and its plot ID.

It contains a GlobalPropertiesWidget for settings applicable to all plot types.

Functionality: It is responsible for displaying the appropriate AbstractPlotPropertiesWidget for the currently selected plot type. It displays this sub-widget in a stacked widget on its canvas.

# Abstract Plot and Properties Architecture
AbstractPlotWidget:

This is a QGraphicsWidget subtype, designed to be rendered in a QGraphicsScene.

It is composed of mostly virtual members and is intended to be subclassed.

It handles events such as mouse clicks.

AbstractPlotParameters:

This is a non-Qt-related type that was recently refactored out.

It contains a pointer to the group manager, a pointer to the table manager, its plot title, and its plot ID (a static integer that is incremented for each new plot).

AbstractPlotPropertiesWidget:

This is a QWidget.

It holds a pointer to the plot widget it is configuring.

It has virtual functions for update and applyToPlot.

It is not Qt-related and handles all of the state. It is unclear if two plots of the same type would each have their own state or if they would use the same widget, causing the state to be copied.

# Concrete Implementation Example: Spatial Overlay
SpatialOverlayPlotWidget:

A subtype of AbstractPlotWidget.

It holds an OpenGLWidget and, as a private member pointer, a QGraphicsProxyWidget. This is likely to maintain the OpenGL canvas within the graphics scene.

It renders different data types using non-widget OpenGL visualization classes, which require a separate canvas.

It has a setGroupManager method, which it uses to pass the group manager pointer on to its OpenGL widget.

SpatialOverlayPlotPropertiesWidget:

A subtype of AbstractPlotPropertiesWidget, defined in the same folder as its corresponding plot widget.

It holds a typed pointer to a SpatialOverlayPlotWidget, rather than the abstract base type.

SpatialOverlayOpenGLWidget:

This is a QOpenGLWidget subclass held by the SpatialOverlayPlotWidget.

It holds the pointers to the actual visualization renderers.

It holds a pointer to the group manager.