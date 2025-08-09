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
    // Configure the view for best results with embedded QOpenGLWidget proxies
    _view->setDragMode(QGraphicsView::RubberBandDrag);
    _view->setRenderHint(QPainter::Antialiasing);
    //_view->setRenderHint(QPainter::HighQualityAntialiasing);
    _view->setViewportUpdateMode(QGraphicsView::FullViewportUpdate);
    _view->setFrameShape(QFrame::NoFrame);
    _view->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    _view->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    _view->setAlignment(Qt::AlignLeft | Qt::AlignTop);
    _view->setOptimizationFlag(QGraphicsView::DontAdjustForAntialiasing, true);
    _view->setOptimizationFlag(QGraphicsView::DontSavePainterState, true);

    // Important: Do NOT set an OpenGL viewport on the QGraphicsView because
    // we embed QOpenGLWidget via QGraphicsProxyWidget inside the scene.
    // Using a GL viewport here can prevent child QOpenGLWidgets from painting.

    // Initialize scene to current size; will be updated on resize
    _scene->setSceneRect(0, 0, width(), height());

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

    // Avoid scaling the view which would rasterize/blurr the embedded GL content
    const QSize viewportSize = _view->viewport()->size();

    // Resize plot item to fill the view
    _plot_item->resize(viewportSize.width(), viewportSize.height());

    // Update scene rect to match the item/view size
    _scene->setSceneRect(0, 0, viewportSize.width(), viewportSize.height());

    // Ensure identity transform for 1:1 pixel mapping
    _view->resetTransform();

    // Keep item at origin
    _plot_item->setPos(0, 0);
}

