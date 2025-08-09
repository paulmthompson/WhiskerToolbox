#include "GraphicsScenePlotOrganizer.hpp"
#include "PlotContainer.hpp"
#include "Plots/AbstractPlotWidget.hpp"
#include "Properties/AbstractPlotPropertiesWidget.hpp"
#include <QGraphicsProxyWidget>
#include <QOpenGLWidget>
#include <QPointF>

GraphicsScenePlotOrganizer::GraphicsScenePlotOrganizer(QObject* parent)
    : AbstractPlotOrganizer(parent)
    , scene_(new QGraphicsScene(this))
    , view_(new QGraphicsView(scene_))
    , default_position_(50, 50)
    , next_position_(default_position_)
{
    // Configure the view: avoid scaling which would rasterize GL proxies
    view_->setDragMode(QGraphicsView::RubberBandDrag);
    view_->setRenderHint(QPainter::Antialiasing);
    //view_->setRenderHint(QPainter::HighQualityAntialiasing);
    view_->setViewportUpdateMode(QGraphicsView::FullViewportUpdate);
    view_->setOptimizationFlag(QGraphicsView::DontAdjustForAntialiasing, true);
    view_->setOptimizationFlag(QGraphicsView::DontSavePainterState, true);
    
    // Do NOT set an OpenGL viewport because plots embed QOpenGLWidget via proxies
    
    // Set a fixed scene rect like the original implementation
    scene_->setSceneRect(0, 0, 1000, 800);
    
    // Set scene background
    scene_->setBackgroundBrush(QBrush(Qt::lightGray));
}

void GraphicsScenePlotOrganizer::addPlot(std::unique_ptr<PlotContainer> plot_container)
{
    if (!plot_container) {
        qDebug() << "GraphicsScenePlotOrganizer::addPlot: null plot_container";
        return;
    }
    
    QString plot_id = plot_container->getPlotId();
    qDebug() << "GraphicsScenePlotOrganizer::addPlot: Adding plot with ID:" << plot_id;
    
    // Add the plot widget to the scene
    AbstractPlotWidget* plot_widget = plot_container->getPlotWidget();
    if (plot_widget) {
        qDebug() << "GraphicsScenePlotOrganizer::addPlot: Adding widget to scene, type:" << plot_widget->getPlotType();
        scene_->addItem(plot_widget);
        plot_widget->setPos(calculateNextPosition());
        qDebug() << "GraphicsScenePlotOrganizer::addPlot: Widget positioned at:" << plot_widget->pos();
    } else {
        qDebug() << "GraphicsScenePlotOrganizer::addPlot: ERROR - null plot_widget";
        return;
    }
    
    // Connect signals
    connectContainerSignals(plot_container.get());
    
    // Store the container
    plot_containers_[plot_id] = std::move(plot_container);
    
    qDebug() << "GraphicsScenePlotOrganizer::addPlot: Successfully added plot, total plots:" << plot_containers_.size();
    emitPlotAdded(plot_id);
}

bool GraphicsScenePlotOrganizer::removePlot(const QString& plot_id)
{
    auto it = plot_containers_.find(plot_id);
    if (it == plot_containers_.end()) {
        return false;
    }
    
    PlotContainer* container = it->second.get();
    
    // Disconnect signals
    disconnectContainerSignals(container);
    
    // Remove plot widget from scene
    AbstractPlotWidget* plot_widget = container->getPlotWidget();
    if (plot_widget) {
        scene_->removeItem(plot_widget);
    }
    
    // Remove from our storage
    plot_containers_.erase(it);
    
    emitPlotRemoved(plot_id);
    return true;
}

PlotContainer* GraphicsScenePlotOrganizer::getPlot(const QString& plot_id) const
{
    auto it = plot_containers_.find(plot_id);
    return (it != plot_containers_.end()) ? it->second.get() : nullptr;
}

QStringList GraphicsScenePlotOrganizer::getAllPlotIds() const
{
    QStringList ids;
    for (auto it = plot_containers_.begin(); it != plot_containers_.end(); ++it) {
        ids.append(it->first);
    }
    return ids;
}

int GraphicsScenePlotOrganizer::getPlotCount() const
{
    return plot_containers_.size();
}

void GraphicsScenePlotOrganizer::selectPlot(const QString& plot_id)
{
    PlotContainer* container = getPlot(plot_id);
    if (!container) {
        return;
    }
    
    AbstractPlotWidget* plot_widget = container->getPlotWidget();
    if (plot_widget) {
        // Clear current selection and select this plot
        scene_->clearSelection();
        plot_widget->setSelected(true);
        
        // Ensure the plot is visible
        view_->centerOn(plot_widget);
    }
    
    emitPlotSelected(plot_id);
}

void GraphicsScenePlotOrganizer::clearAllPlots()
{
    // Get all plot IDs before clearing (to avoid iterator invalidation)
    QStringList plot_ids = getAllPlotIds();
    
    for (const QString& plot_id : plot_ids) {
        removePlot(plot_id);
    }
}

QWidget* GraphicsScenePlotOrganizer::getDisplayWidget()
{
    return view_;
}

void GraphicsScenePlotOrganizer::setDefaultPlotPosition(const QPointF& position)
{
    default_position_ = position;
    next_position_ = position;
}

void GraphicsScenePlotOrganizer::arrangeInGrid()
{
    const qreal plot_width = 300;  // Estimated plot width
    const qreal plot_height = 200; // Estimated plot height
    const qreal spacing = 20;
    
    int plots_per_row = 3; // Adjust based on available space
    int row = 0;
    int col = 0;
    
    for (auto& pair : plot_containers_) {
        AbstractPlotWidget* plot_widget = pair.second->getPlotWidget();
        if (plot_widget) {
            qreal x = col * (plot_width + spacing);
            qreal y = row * (plot_height + spacing);
            plot_widget->setPos(x, y);
            
            col++;
            if (col >= plots_per_row) {
                col = 0;
                row++;
            }
        }
    }
    
    ensurePlotsVisible();
}

void GraphicsScenePlotOrganizer::ensurePlotsVisible()
{
    if (plot_containers_.empty()) {
        return;
    }
    
    // Use fixed scene rect like the original - don't change it dynamically
    QRectF scene_rect = scene_->sceneRect();
    
    for (auto& pair : plot_containers_) {
        AbstractPlotWidget* plot_widget = pair.second->getPlotWidget();
        if (plot_widget) {
            QRectF plot_bounds = plot_widget->boundingRect();
            QPointF plot_pos = plot_widget->pos();
            
            // Check if plot is outside scene bounds
            if (plot_pos.x() < scene_rect.left() || 
                plot_pos.x() + plot_bounds.width() > scene_rect.right() ||
                plot_pos.y() < scene_rect.top() || 
                plot_pos.y() + plot_bounds.height() > scene_rect.bottom()) {
                
                // Move plot to center of scene
                qreal center_x = scene_rect.center().x() - plot_bounds.width() / 2;
                qreal center_y = scene_rect.center().y() - plot_bounds.height() / 2;
                
                // Clamp to scene bounds
                center_x = qMax(scene_rect.left(), qMin(center_x, scene_rect.right() - plot_bounds.width()));
                center_y = qMax(scene_rect.top(), qMin(center_y, scene_rect.bottom() - plot_bounds.height()));
                
                plot_widget->setPos(QPointF(center_x, center_y));
                qDebug() << "GraphicsScenePlotOrganizer: Moved plot" << pair.first << "to" << QPointF(center_x, center_y);
            }
        }
    }
}

void GraphicsScenePlotOrganizer::onPlotSelected(const QString& plot_id)
{
    qDebug() << "GraphicsScenePlotOrganizer::onPlotSelected called with plot_id:" << plot_id;
    emitPlotSelected(plot_id);
}

void GraphicsScenePlotOrganizer::onFrameJumpRequested(int64_t time_frame_index, const std::string& data_key)
{
    emitFrameJumpRequested(time_frame_index, data_key);
}

QPointF GraphicsScenePlotOrganizer::calculateNextPosition()
{
    QPointF position = next_position_;
    
    // Simple positioning logic - could be more sophisticated
    next_position_.setX(next_position_.x() + 320); // Offset for next plot
    if (next_position_.x() > 1000) { // Wrap to next row
        next_position_.setX(default_position_.x());
        next_position_.setY(next_position_.y() + 220);
    }
    
    return position;
}

void GraphicsScenePlotOrganizer::connectContainerSignals(PlotContainer* container)
{
    if (!container) {
        return;
    }
    
    connect(container, &PlotContainer::plotSelected,
            this, &GraphicsScenePlotOrganizer::onPlotSelected);
    connect(container, &PlotContainer::frameJumpRequested,
            this, &GraphicsScenePlotOrganizer::onFrameJumpRequested);
}

void GraphicsScenePlotOrganizer::disconnectContainerSignals(PlotContainer* container)
{
    if (!container) {
        return;
    }
    
    disconnect(container, &PlotContainer::plotSelected,
               this, &GraphicsScenePlotOrganizer::onPlotSelected);
    disconnect(container, &PlotContainer::frameJumpRequested,
               this, &GraphicsScenePlotOrganizer::onFrameJumpRequested);
}
