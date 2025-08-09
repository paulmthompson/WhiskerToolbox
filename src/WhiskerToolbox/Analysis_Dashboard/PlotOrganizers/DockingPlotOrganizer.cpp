#include "DockingPlotOrganizer.hpp"
#include "PlotContainer.hpp"
#include "Plots/AbstractPlotWidget.hpp"
#include "PlotDockWidget.hpp"
#include "PlotDockWidgetContent.hpp"

// Third-party
#include "DockManager.h"
#include "DockAreaWidget.h"

#include <QDebug>
#include <QOpenGLWidget>

DockingPlotOrganizer::DockingPlotOrganizer(ads::CDockManager* dock_manager, QObject* parent)
    : AbstractPlotOrganizer(parent)
    , _dock_manager(dock_manager)
    , _placeholder_widget(new QWidget())
{
    // Placeholder is a minimal widget to satisfy the dashboard center panel
}

DockingPlotOrganizer::~DockingPlotOrganizer() = default;

void DockingPlotOrganizer::addPlot(std::unique_ptr<PlotContainer> plot_container)
{
    if (!_dock_manager) {
        qWarning() << "DockingPlotOrganizer::addPlot: dock manager is null";
        return;
    }
    if (!plot_container) {
        qWarning() << "DockingPlotOrganizer::addPlot: null plot_container";
        return;
    }

    const QString plot_id = plot_container->getPlotId();
    AbstractPlotWidget* plot_item = plot_container->getPlotWidget();
    if (!plot_item) {
        qWarning() << "DockingPlotOrganizer::addPlot: null plot item for" << plot_id;
        return;
    }

    // Hide internal frame/title when docked so content fills the dock
    plot_item->setFrameAndTitleVisible(false);

    // Create per-plot content widget (scene + view + GL viewport)
    auto* content = new PlotDockWidgetContent(plot_id, plot_item);

    // Wrap in a dock widget
    auto* dock_widget = new PlotDockWidget(plot_id, content);

    // Add to dock manager, let ADS choose placement
    _dock_manager->addDockWidget(ads::RightDockWidgetArea, dock_widget);

    // Connect activations (focus/click anywhere) to select
    QObject::connect(content, &PlotDockWidgetContent::activated,
                     this, &DockingPlotOrganizer::_onContentActivated);

    // Connect close request to removal
    QObject::connect(dock_widget, &PlotDockWidget::closeRequested,
                     this, &DockingPlotOrganizer::_onDockCloseRequested);

    // Connect container signals up to organizer
    _connectContainerSignals(plot_container.get());

    // Store entry
    Entry entry;
    entry._container = std::move(plot_container);
    entry._content = content;
    entry._dock = dock_widget;
    _entries.emplace(plot_id, std::move(entry));

    emitPlotAdded(plot_id);
}

bool DockingPlotOrganizer::removePlot(QString const& plot_id)
{
    auto it = _entries.find(plot_id);
    if (it == _entries.end()) {
        return false;
    }

    PlotContainer* container = it->second._container.get();
    _disconnectContainerSignals(container);

    // Remove dock widget without re-emitting closeRequested
    if (it->second._dock) {
        it->second._dock->blockSignals(true);
        it->second._dock->close();
        it->second._dock->blockSignals(false);
        it->second._dock->deleteLater();
    }

    _entries.erase(it);
    emitPlotRemoved(plot_id);
    return true;
}

PlotContainer* DockingPlotOrganizer::getPlot(QString const& plot_id) const
{
    auto it = _entries.find(plot_id);
    return (it != _entries.end()) ? it->second._container.get() : nullptr;
}

QStringList DockingPlotOrganizer::getAllPlotIds() const
{
    QStringList ids;
    ids.reserve(static_cast<int>(_entries.size()));
    for (auto const& kv : _entries) {
        ids.append(kv.first);
    }
    return ids;
}

int DockingPlotOrganizer::getPlotCount() const
{
    return static_cast<int>(_entries.size());
}

void DockingPlotOrganizer::selectPlot(QString const& plot_id)
{
    auto it = _entries.find(plot_id);
    if (it == _entries.end()) {
        return;
    }

    // Show and focus the dock
    if (auto* dock = it->second._dock) {
        dock->show();
        if (auto* content = it->second._content) {
            content->setFocus(Qt::OtherFocusReason);
        }
    }

    emitPlotSelected(plot_id);
}

void DockingPlotOrganizer::clearAllPlots()
{
    // Copy keys to avoid iterator invalidation
    QStringList ids = getAllPlotIds();
    for (auto const& id : ids) {
        removePlot(id);
    }
}

QWidget* DockingPlotOrganizer::getDisplayWidget()
{
    return _placeholder_widget;
}

void DockingPlotOrganizer::_onContentActivated(QString const& plot_id)
{
    selectPlot(plot_id);
}

void DockingPlotOrganizer::_onDockCloseRequested(QString const& plot_id)
{
    // Cleanup without recursive close calls
    auto it = _entries.find(plot_id);
    if (it == _entries.end()) {
        return;
    }
    PlotContainer* container = it->second._container.get();
    _disconnectContainerSignals(container);
    if (it->second._dock) {
        it->second._dock->deleteLater();
    }
    _entries.erase(it);
    emitPlotRemoved(plot_id);
}

void DockingPlotOrganizer::_connectContainerSignals(PlotContainer* container)
{
    if (!container) { return; }
    QObject::connect(container, &PlotContainer::plotSelected,
                     this, &DockingPlotOrganizer::_onPlotSelected);
    QObject::connect(container, &PlotContainer::frameJumpRequested,
                     this, &DockingPlotOrganizer::_onFrameJumpRequested);
}

void DockingPlotOrganizer::_disconnectContainerSignals(PlotContainer* container)
{
    if (!container) { return; }
    QObject::disconnect(container, &PlotContainer::plotSelected,
                        this, &DockingPlotOrganizer::_onPlotSelected);
    QObject::disconnect(container, &PlotContainer::frameJumpRequested,
                        this, &DockingPlotOrganizer::_onFrameJumpRequested);

}

void DockingPlotOrganizer::_onPlotSelected(QString const& plot_id)
{
    emitPlotSelected(plot_id);
}

void DockingPlotOrganizer::_onFrameJumpRequested(int64_t time_frame_index, const std::string& data_key)
{
    emitFrameJumpRequested(time_frame_index, data_key);
}

