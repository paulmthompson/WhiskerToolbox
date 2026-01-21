#include "TestWidgetView.hpp"
#include "TestWidgetState.hpp"

#include <QGraphicsTextItem>
#include <QGraphicsEllipseItem>
#include <QGraphicsLineItem>
#include <QResizeEvent>
#include <QPen>
#include <QFont>

#include <cmath>

TestWidgetView::TestWidgetView(std::shared_ptr<TestWidgetState> state,
                               QWidget * parent)
    : QGraphicsView(parent)
    , _state(std::move(state))
    , _scene(new QGraphicsScene(this))
    , _animation_timer(new QTimer(this)) {
    
    setScene(_scene);
    setRenderHint(QPainter::Antialiasing);
    setViewportUpdateMode(QGraphicsView::FullViewportUpdate);
    setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    setBackgroundBrush(QBrush(QColor(40, 40, 40)));
    
    // Set minimum size for the view
    setMinimumSize(200, 150);
    
    // Setup animation timer
    connect(_animation_timer, &QTimer::timeout, this, &TestWidgetView::onAnimationTick);
    
    // Connect state signals
    connectStateSignals();
    
    // Initial setup
    setupScene();
}

TestWidgetView::~TestWidgetView() {
    _animation_timer->stop();
}

void TestWidgetView::connectStateSignals() {
    if (!_state) {
        return;
    }
    
    connect(_state.get(), &TestWidgetState::showGridChanged,
            this, &TestWidgetView::onShowGridChanged);
    connect(_state.get(), &TestWidgetState::showCrosshairChanged,
            this, &TestWidgetView::onShowCrosshairChanged);
    connect(_state.get(), &TestWidgetState::enableAnimationChanged,
            this, &TestWidgetView::onEnableAnimationChanged);
    connect(_state.get(), &TestWidgetState::highlightColorChanged,
            this, &TestWidgetView::onHighlightColorChanged);
    connect(_state.get(), &TestWidgetState::zoomLevelChanged,
            this, &TestWidgetView::onZoomLevelChanged);
    connect(_state.get(), &TestWidgetState::gridSpacingChanged,
            this, &TestWidgetView::onGridSpacingChanged);
    connect(_state.get(), &TestWidgetState::labelTextChanged,
            this, &TestWidgetView::onLabelTextChanged);
}

void TestWidgetView::setupScene() {
    rebuildScene();
}

void TestWidgetView::rebuildScene() {
    // Clear existing items
    _scene->clear();
    _grid_items.clear();
    _crosshair_items.clear();
    _label_item = nullptr;
    _animated_circle = nullptr;
    
    if (!_state) {
        return;
    }
    
    // Set scene rect based on view size
    QRectF const scene_rect(-200, -150, 400, 300);
    _scene->setSceneRect(scene_rect);
    
    // Create grid
    updateGrid();
    
    // Create crosshair
    updateCrosshair();
    
    // Create label
    _label_item = _scene->addText(_state->labelText());
    updateLabel();
    
    // Create animated circle (initially hidden)
    _animated_circle = _scene->addEllipse(-10, -10, 20, 20,
                                          QPen(Qt::NoPen),
                                          QBrush(_state->highlightColor()));
    _animated_circle->setVisible(_state->enableAnimation());
    _animated_circle->setPos(80, 0);  // Initial position on circle
    
    // Start animation if enabled
    if (_state->enableAnimation()) {
        _animation_timer->start(30);  // ~33 FPS
    }
    
    // Apply zoom
    updateZoom();
}

void TestWidgetView::updateGrid() {
    if (!_state) {
        return;
    }
    
    // Remove existing grid items
    for (auto * item : _grid_items) {
        _scene->removeItem(item);
        delete item;
    }
    _grid_items.clear();
    
    if (!_state->showGrid()) {
        return;
    }
    
    int const spacing = _state->gridSpacing();
    QRectF const rect = _scene->sceneRect();
    QPen const grid_pen(QColor(80, 80, 80), 1);
    
    // Vertical lines
    for (int x = static_cast<int>(rect.left()); x <= rect.right(); x += spacing) {
        auto * line = _scene->addLine(x, rect.top(), x, rect.bottom(), grid_pen);
        _grid_items.append(line);
    }
    
    // Horizontal lines
    for (int y = static_cast<int>(rect.top()); y <= rect.bottom(); y += spacing) {
        auto * line = _scene->addLine(rect.left(), y, rect.right(), y, grid_pen);
        _grid_items.append(line);
    }
}

void TestWidgetView::updateCrosshair() {
    if (!_state) {
        return;
    }
    
    // Remove existing crosshair items
    for (auto * item : _crosshair_items) {
        _scene->removeItem(item);
        delete item;
    }
    _crosshair_items.clear();
    
    if (!_state->showCrosshair()) {
        return;
    }
    
    QRectF const rect = _scene->sceneRect();
    QPen const crosshair_pen(_state->highlightColor(), 2);
    
    // Horizontal line through center
    auto * h_line = _scene->addLine(rect.left(), 0, rect.right(), 0, crosshair_pen);
    _crosshair_items.append(h_line);
    
    // Vertical line through center
    auto * v_line = _scene->addLine(0, rect.top(), 0, rect.bottom(), crosshair_pen);
    _crosshair_items.append(v_line);
}

void TestWidgetView::updateLabel() {
    if (!_label_item || !_state) {
        return;
    }
    
    _label_item->setPlainText(_state->labelText());
    _label_item->setDefaultTextColor(_state->highlightColor());
    
    QFont font;
    font.setPointSize(14);
    font.setBold(true);
    _label_item->setFont(font);
    
    // Position label at top-left area
    _label_item->setPos(-180, -130);
}

void TestWidgetView::updateZoom() {
    if (!_state) {
        return;
    }
    
    resetTransform();
    scale(_state->zoomLevel(), _state->zoomLevel());
}

void TestWidgetView::resizeEvent(QResizeEvent * event) {
    QGraphicsView::resizeEvent(event);
    
    // Update scene rect based on new size
    int const w = event->size().width();
    int const h = event->size().height();
    _scene->setSceneRect(-w / 2, -h / 2, w, h);
    
    // Rebuild scene elements
    rebuildScene();
}

// === State Change Handlers ===

void TestWidgetView::onShowGridChanged(bool /*show*/) {
    updateGrid();
}

void TestWidgetView::onShowCrosshairChanged(bool /*show*/) {
    updateCrosshair();
}

void TestWidgetView::onEnableAnimationChanged(bool enable) {
    if (_animated_circle) {
        _animated_circle->setVisible(enable);
    }
    
    if (enable) {
        _animation_timer->start(30);
    } else {
        _animation_timer->stop();
    }
}

void TestWidgetView::onHighlightColorChanged(QColor const & color) {
    // Update crosshair color
    for (auto * item : _crosshair_items) {
        if (auto * line = dynamic_cast<QGraphicsLineItem *>(item)) {
            QPen pen = line->pen();
            pen.setColor(color);
            line->setPen(pen);
        }
    }
    
    // Update label color
    if (_label_item) {
        _label_item->setDefaultTextColor(color);
    }
    
    // Update animated circle color
    if (_animated_circle) {
        _animated_circle->setBrush(QBrush(color));
    }
}

void TestWidgetView::onZoomLevelChanged(double /*zoom*/) {
    updateZoom();
}

void TestWidgetView::onGridSpacingChanged(int /*spacing*/) {
    updateGrid();
}

void TestWidgetView::onLabelTextChanged(QString const & /*text*/) {
    updateLabel();
}

void TestWidgetView::onAnimationTick() {
    if (!_animated_circle || !_state || !_state->enableAnimation()) {
        return;
    }
    
    // Move circle in a circular path
    _animation_angle += 0.05;
    if (_animation_angle > 2 * M_PI) {
        _animation_angle -= 2 * M_PI;
    }
    
    double const radius = 80.0;
    double const x = radius * std::cos(_animation_angle);
    double const y = radius * std::sin(_animation_angle);
    
    _animated_circle->setPos(x, y);
}
