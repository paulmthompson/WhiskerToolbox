#ifndef TEST_WIDGET_VIEW_HPP
#define TEST_WIDGET_VIEW_HPP

/**
 * @file TestWidgetView.hpp
 * @brief View component for TestWidget (View/Properties split proof-of-concept)
 * 
 * TestWidgetView is the visualization component that displays:
 * - A canvas with optional grid overlay
 * - Optional crosshair at center
 * - A colored label
 * - Optional animated element
 * 
 * All display is driven by TestWidgetState. When state changes,
 * the view automatically updates via Qt signal connections.
 * 
 * ## View/Properties Split Pattern
 * 
 * This class demonstrates the "View" side of the split:
 * - Receives state via shared_ptr
 * - Connects to state signals to update display
 * - Does NOT provide controls to modify state (that's TestWidgetProperties)
 * - May emit user interaction signals that the widget can use to modify state
 * 
 * @see TestWidgetState for the shared state
 * @see TestWidgetProperties for the properties/controls component
 */

#include <QGraphicsView>
#include <QGraphicsScene>
#include <QTimer>

#include <memory>

class TestWidgetState;

/**
 * @brief View component for TestWidget
 * 
 * Displays a simple canvas demonstrating state-driven rendering:
 * - Grid lines (toggled by state->showGrid())
 * - Crosshair (toggled by state->showCrosshair())
 * - Animated circle (toggled by state->enableAnimation())
 * - Label with highlight color (from state->labelText(), state->highlightColor())
 * - Zoom level (from state->zoomLevel())
 * - Grid spacing (from state->gridSpacing())
 */
class TestWidgetView : public QGraphicsView {
    Q_OBJECT

public:
    /**
     * @brief Construct a new TestWidgetView
     * @param state Shared pointer to the state (shared with TestWidgetProperties)
     * @param parent Parent widget
     */
    explicit TestWidgetView(std::shared_ptr<TestWidgetState> state,
                           QWidget * parent = nullptr);

    ~TestWidgetView() override;

protected:
    void resizeEvent(QResizeEvent * event) override;

private slots:
    // === State Change Handlers ===
    void onShowGridChanged(bool show);
    void onShowCrosshairChanged(bool show);
    void onEnableAnimationChanged(bool enable);
    void onHighlightColorChanged(QColor const & color);
    void onZoomLevelChanged(double zoom);
    void onGridSpacingChanged(int spacing);
    void onLabelTextChanged(QString const & text);

    // === Animation ===
    void onAnimationTick();

private:
    void setupScene();
    void connectStateSignals();
    void updateGrid();
    void updateCrosshair();
    void updateLabel();
    void updateZoom();
    void rebuildScene();

    std::shared_ptr<TestWidgetState> _state;
    QGraphicsScene * _scene;

    // Scene items
    QList<QGraphicsItem *> _grid_items;
    QList<QGraphicsItem *> _crosshair_items;
    QGraphicsTextItem * _label_item = nullptr;
    QGraphicsEllipseItem * _animated_circle = nullptr;

    // Animation
    QTimer * _animation_timer;
    double _animation_angle = 0.0;
};

#endif // TEST_WIDGET_VIEW_HPP
