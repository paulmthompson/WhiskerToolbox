#include "ScatterPlotWidget.hpp"
#include "ScatterPlotOpenGLWidget.hpp"

#include <QDebug>
#include <QGraphicsProxyWidget>
#include <QGraphicsSceneMouseEvent>
#include <QPainter>
#include <QStyleOptionGraphicsItem>

ScatterPlotWidget::ScatterPlotWidget(QGraphicsItem * parent)
    : AbstractPlotWidget(parent),
      _opengl_widget(nullptr),
      _proxy_widget(nullptr) {
    qDebug() << "ScatterPlotWidget::ScatterPlotWidget constructor called";

    setPlotTitle("Scatter Plot");
    
    // Disable item caching to ensure live updates from embedded OpenGL widget
    setCacheMode(QGraphicsItem::NoCache);

    setupOpenGLWidget();

    qDebug() << "ScatterPlotWidget::ScatterPlotWidget constructor done";
}

ScatterPlotWidget::~ScatterPlotWidget() {
    qDebug() << "ScatterPlotWidget::~ScatterPlotWidget destructor called";
}

QString ScatterPlotWidget::getPlotType() const {
    return "Scatter Plot";
}

void ScatterPlotWidget::setScatterData(std::vector<float> const & x_data, 
                                      std::vector<float> const & y_data) {
    qDebug() << "ScatterPlotWidget::setScatterData called with" 
             << x_data.size() << "x points and" << y_data.size() << "y points";
             
    if (_opengl_widget) {
        qDebug() << "ScatterPlotWidget: Forwarding data to OpenGL widget";
        _opengl_widget->setScatterData(x_data, y_data);
        emit renderingPropertiesChanged();
        qDebug() << "ScatterPlotWidget: Data set and signal emitted";
    } else {
        qDebug() << "ScatterPlotWidget: No OpenGL widget available!";
    }
}

void ScatterPlotWidget::setAxisLabels(QString const & x_label, QString const & y_label) {
    if (_opengl_widget) {
        _opengl_widget->setAxisLabels(x_label, y_label);
    }
}

void ScatterPlotWidget::setGroupManager(GroupManager * group_manager) {
    if (_opengl_widget) {
        _opengl_widget->setGroupManager(group_manager);
    }
}

void ScatterPlotWidget::setPointSize(float point_size) {
    if (_opengl_widget) {
        _opengl_widget->setPointSize(point_size);
        emit renderingPropertiesChanged();
    }
}

float ScatterPlotWidget::getPointSize() const {
    if (_opengl_widget) {
        return _opengl_widget->getPointSize();
    }
    return 3.0f; // Default value
}

void ScatterPlotWidget::setPanOffset(float offset_x, float offset_y) {
    if (_opengl_widget) {
        _opengl_widget->setPanOffset(offset_x, offset_y);
    }
}

QVector2D ScatterPlotWidget::getPanOffset() const {
    if (_opengl_widget) {
        return _opengl_widget->getPanOffset();
    }
    return QVector2D(0.0f, 0.0f); // Default value
}

void ScatterPlotWidget::setTooltipsEnabled(bool enabled) {
    if (_opengl_widget) {
        _opengl_widget->setTooltipsEnabled(enabled);
    }
}

bool ScatterPlotWidget::getTooltipsEnabled() const {
    if (_opengl_widget) {
        return _opengl_widget->getTooltipsEnabled();
    }
    return true; // Default value
}

void ScatterPlotWidget::paint(QPainter * painter, QStyleOptionGraphicsItem const * option, QWidget * widget) {
    // The actual rendering is handled by the OpenGL widget
    // This method is called for the widget's background/border
    Q_UNUSED(option)
    Q_UNUSED(widget)

    if (isFrameAndTitleVisible()) {
        // Draw frame around the plot
        QRectF rect = boundingRect();

        QPen border_pen;
        if (isSelected()) {
            border_pen.setColor(QColor(0, 120, 200));
            border_pen.setWidth(2);
        } else {
            border_pen.setColor(QColor(100, 100, 100));
            border_pen.setWidth(1);
        }
        painter->setPen(border_pen);
        painter->drawRect(rect);

        // Draw title
        painter->setPen(QColor(0, 0, 0));
        QFont title_font = painter->font();
        title_font.setBold(true);
        painter->setFont(title_font);

        QRectF title_rect = rect.adjusted(5, 5, -5, -rect.height() + 20);
        painter->drawText(title_rect, Qt::AlignCenter, getPlotTitle());
    }
}

void ScatterPlotWidget::resizeEvent(QGraphicsSceneResizeEvent * event) {
    AbstractPlotWidget::resizeEvent(event);
    
    if (_proxy_widget) {
        // Update the proxy widget to fill the plot area
        QRectF rect = boundingRect();
        QRectF content_rect = isFrameAndTitleVisible()
            ? rect.adjusted(8, 30, -8, -8) // Account for resize handles and title
            : rect.adjusted(0, 0, 0, 0);
        _proxy_widget->setGeometry(content_rect);
        _proxy_widget->widget()->resize(content_rect.size().toSize());
    }
}

void ScatterPlotWidget::mousePressEvent(QGraphicsSceneMouseEvent * event) {
    // Check if the click is in the title area (top 25 pixels)
    QRectF title_area = boundingRect().adjusted(0, 0, 0, -boundingRect().height() + 25);

    if (title_area.contains(event->pos())) {
        // Click in title area - handle selection and allow movement
        emit plotSelected(getPlotId());
        // Make sure the item is movable for dragging
        setFlag(QGraphicsItem::ItemIsMovable, true);
        AbstractPlotWidget::mousePressEvent(event);
    } else {
        // Click in content area - let the OpenGL widget handle it
        // But still emit selection signal
        emit plotSelected(getPlotId());
        // Disable movement when clicking in content area
        setFlag(QGraphicsItem::ItemIsMovable, false);
        // Don't call parent implementation to avoid interfering with OpenGL panning
        event->accept();
    }
}

void ScatterPlotWidget::updateVisualization() {
    // This method can be called when data sources change
    // For now, it's a placeholder for future data loading logic
    emit renderingPropertiesChanged();
}

void ScatterPlotWidget::setupOpenGLWidget() {
    // Create the OpenGL widget
    _opengl_widget = new ScatterPlotOpenGLWidget();
    
    // Create a proxy widget to embed the OpenGL widget in the graphics scene
    _proxy_widget = new QGraphicsProxyWidget(this);
    _proxy_widget->setWidget(_opengl_widget);

    // Configure OpenGL widget for better compatibility inside QGraphicsScene
    _opengl_widget->setAttribute(Qt::WA_AlwaysStackOnTop, false);
    _opengl_widget->setAttribute(Qt::WA_OpaquePaintEvent, true);
    _opengl_widget->setAttribute(Qt::WA_NoSystemBackground, true);
    _opengl_widget->setUpdateBehavior(QOpenGLWidget::NoPartialUpdate);

    // Configure the proxy widget to not interfere with parent interactions
    _proxy_widget->setFlag(QGraphicsItem::ItemIsMovable, false);
    _proxy_widget->setFlag(QGraphicsItem::ItemIsSelectable, false);

    // Disable proxy caching so that it repaints each update from the child widget
    _proxy_widget->setCacheMode(QGraphicsItem::NoCache);
    
    // Position the proxy widget within this graphics item
    // Leave space for title, border, and resize handles
    QRectF rect = boundingRect();
    QRectF content_rect = isFrameAndTitleVisible()
        ? rect.adjusted(8, 30, -8, -8)
        : rect;
    _proxy_widget->setGeometry(content_rect);
    
    // Connect signals after widget is created
    connectOpenGLSignals();
    
    qDebug() << "ScatterPlotWidget: OpenGL widget setup complete";
}

void ScatterPlotWidget::connectOpenGLSignals() {
    if (!_opengl_widget) {
        return;
    }
    
    // Connect OpenGL widget signals to this widget's signals
    connect(_opengl_widget, &ScatterPlotOpenGLWidget::pointClicked,
            this, &ScatterPlotWidget::pointClicked);
    
    // Connect property change signals
    connect(_opengl_widget, &ScatterPlotOpenGLWidget::zoomLevelChanged,
            this, &ScatterPlotWidget::renderingPropertiesChanged);
    connect(_opengl_widget, &ScatterPlotOpenGLWidget::panOffsetChanged,
            this, &ScatterPlotWidget::renderingPropertiesChanged);

    // Force proxy and item repaints on interactive changes to ensure on-screen updates
    auto requestRepaint = [this]() {
        if (_proxy_widget) {
            _proxy_widget->update();
        }
        this->update();
    };
    connect(_opengl_widget, &ScatterPlotOpenGLWidget::zoomLevelChanged, this, requestRepaint);
    connect(_opengl_widget, &ScatterPlotOpenGLWidget::panOffsetChanged, this, requestRepaint);
    
    qDebug() << "ScatterPlotWidget: OpenGL signals connected";
}