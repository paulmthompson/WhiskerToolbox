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

void ScatterPlotWidget::setZoomLevel(float zoom_level) {
    if (_opengl_widget) {
        _opengl_widget->setZoomLevel(zoom_level);
    }
}

float ScatterPlotWidget::getZoomLevel() const {
    if (_opengl_widget) {
        return _opengl_widget->getZoomLevel();
    }
    return 1.0f; // Default value
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

void ScatterPlotWidget::resizeEvent(QGraphicsSceneResizeEvent * event) {
    AbstractPlotWidget::resizeEvent(event);
    
    if (_proxy_widget) {
        // Update the proxy widget to fill the plot area (minus title space)
        QRectF rect = boundingRect();
        QRectF content_rect = rect.adjusted(1, 25, -1, -1); // Leave space for title and border
        _proxy_widget->setGeometry(content_rect);
        _proxy_widget->widget()->resize(content_rect.size().toSize());
    }
}

void ScatterPlotWidget::mousePressEvent(QGraphicsSceneMouseEvent * event) {
    // Let the base class handle the event first
    AbstractPlotWidget::mousePressEvent(event);
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
    
    // Position the proxy widget within this graphics item
    QRectF rect = boundingRect();
    QRectF content_rect = rect.adjusted(1, 25, -1, -1); // Leave space for title and border
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
    
    qDebug() << "ScatterPlotWidget: OpenGL signals connected";
}