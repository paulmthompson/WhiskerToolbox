#include "PlotDockWidgetContent.hpp"
#include "Plots/AbstractPlotWidget.hpp"

#include <QFocusEvent>
#include <QGraphicsScene>
#include <QGraphicsView>
#include <QOpenGLWidget>
#include <QVBoxLayout>
#include <QEvent>
#include <QResizeEvent>

PlotDockWidgetContent::PlotDockWidgetContent(QString const& plot_id,
                                             AbstractPlotWidget* plot_item,
                                             QWidget* parent)
    : QWidget(parent)
    , _plot_id(plot_id)
    , _scene(new QGraphicsScene(this))
    , _view(new QGraphicsView(_scene, this))
{
    setObjectName(QStringLiteral("PlotDockWidgetContent_%1").arg(plot_id));
    setFocusPolicy(Qt::StrongFocus);

    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);
    layout->addWidget(_view);

    // Install event filters to treat clicks as activation
    installEventFilter(this);
    _view->installEventFilter(this);

    _initView(plot_item);
}

void PlotDockWidgetContent::_initView(AbstractPlotWidget* plot_item)
{
    // Configure the view similar to GraphicsScenePlotOrganizer
    _view->setDragMode(QGraphicsView::RubberBandDrag);
    _view->setRenderHint(QPainter::Antialiasing);
    _view->setViewportUpdateMode(QGraphicsView::SmartViewportUpdate);
    _view->setFrameShape(QFrame::NoFrame);
    _view->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    _view->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);

    // Use OpenGL viewport
    auto* gl_viewport = new QOpenGLWidget();
    _view->setViewport(gl_viewport);

    // Scene bounds can be flexible; set a generous rect
    _scene->setSceneRect(0, 0, 1600, 1200);

    if (plot_item) {
        _plot_item = plot_item;
        _scene->addItem(_plot_item);
        _plot_item->setPos(0, 0);
        _fitPlotToView();
    }
}

void PlotDockWidgetContent::focusInEvent(QFocusEvent* event)
{
    QWidget::focusInEvent(event);
    emit activated(_plot_id);
}

bool PlotDockWidgetContent::eventFilter(QObject* watched, QEvent* event)
{
    switch (event->type()) {
        case QEvent::MouseButtonPress:
        case QEvent::MouseButtonDblClick:
        case QEvent::FocusIn:
            emit activated(_plot_id);
            break;
        default:
            break;
    }
    return QWidget::eventFilter(watched, event);
}

void PlotDockWidgetContent::resizeEvent(QResizeEvent* event)
{
    QWidget::resizeEvent(event);
    _fitPlotToView();
}

void PlotDockWidgetContent::_fitPlotToView()
{
    if (!_plot_item) { return; }

    // Compute the item bounding rect in scene coordinates
    QRectF itemRect = _plot_item->boundingRect();
    // Make sure the scene is at least the item size
    _scene->setSceneRect(itemRect);

    // Fit the entire scene (plot) into the view with aspect ratio preserved
    _view->fitInView(_scene->sceneRect(), Qt::KeepAspectRatio);
}

