#include "GroupCoordinator.hpp"
#include "Groups/GroupManager.hpp"
#include "Plots/AbstractPlotWidget.hpp"

#include <QDebug>
#include <QColor>

GroupCoordinator::GroupCoordinator(GroupManager* group_manager, QObject* parent)
    : QObject(parent)
    , group_manager_(group_manager)
{
    if (group_manager_) {
        connectToGroupManager();
        qDebug() << "GroupCoordinator: Initialized with GroupManager";
    } else {
        qWarning() << "GroupCoordinator: Initialized with null GroupManager";
    }
}

void GroupCoordinator::registerPlot(const QString& plot_id, AbstractPlotWidget* plot_widget)
{
    if (!plot_widget) {
        qWarning() << "GroupCoordinator::registerPlot: null plot_widget for plot_id:" << plot_id;
        return;
    }

    if (registered_plots_.contains(plot_id)) {
        qDebug() << "GroupCoordinator::registerPlot: Plot already registered:" << plot_id;
        return;
    }

    registered_plots_[plot_id] = plot_widget;
    connectToPlot(plot_widget);
    
    qDebug() << "GroupCoordinator::registerPlot: Registered plot" << plot_id 
             << "Total registered plots:" << registered_plots_.size();
}

void GroupCoordinator::unregisterPlot(const QString& plot_id)
{
    auto it = registered_plots_.find(plot_id);
    if (it == registered_plots_.end()) {
        qDebug() << "GroupCoordinator::unregisterPlot: Plot not found:" << plot_id;
        return;
    }

    AbstractPlotWidget* plot_widget = it.value();
    disconnectFromPlot(plot_widget);
    registered_plots_.erase(it);

    qDebug() << "GroupCoordinator::unregisterPlot: Unregistered plot" << plot_id
             << "Remaining plots:" << registered_plots_.size();
}

bool GroupCoordinator::isPlotRegistered(const QString& plot_id) const
{
    return registered_plots_.contains(plot_id);
}

void GroupCoordinator::onGroupSelectionChanged(int group_id, bool selected)
{
    qDebug() << "GroupCoordinator::onGroupSelectionChanged: Group" << group_id << "selected:" << selected;
    
    if (selected) {
        currently_selected_groups_.insert(group_id);
    } else {
        currently_selected_groups_.remove(group_id);
    }
    
    // Broadcast to all registered plots
    emit groupSelectionChanged(group_id, selected);
    
    // Also notify all plots through their interfaces if they have group selection methods
    for (auto it = registered_plots_.begin(); it != registered_plots_.end(); ++it) {
        AbstractPlotWidget* plot = it.value();
        if (plot) {
            // Note: We'll need to add these methods to AbstractPlotWidget
            // For now, we rely on the signal/slot mechanism
        }
    }
}

void GroupCoordinator::onGroupCreated(int group_id, const QString& group_name, const QColor& group_color)
{
    qDebug() << "GroupCoordinator::onGroupCreated: Group" << group_id << "name:" << group_name;
    
    emit groupCreated(group_id, group_name, group_color);
}

void GroupCoordinator::onGroupRemoved(int group_id)
{
    qDebug() << "GroupCoordinator::onGroupRemoved: Group" << group_id;
    
    // Clean up our tracking sets
    currently_selected_groups_.remove(group_id);
    currently_highlighted_groups_.remove(group_id);
    
    emit groupRemoved(group_id);
}

void GroupCoordinator::onGroupPropertiesChanged(int group_id)
{
    qDebug() << "GroupCoordinator::onGroupPropertiesChanged: Group" << group_id;
    
    emit groupPropertiesChanged(group_id);
}

void GroupCoordinator::onGroupHighlightRequested(int group_id, bool highlight, const QString& requesting_plot_id)
{
    qDebug() << "GroupCoordinator::onGroupHighlightRequested: Group" << group_id 
             << "highlight:" << highlight << "from plot:" << requesting_plot_id;
    
    if (highlight) {
        currently_highlighted_groups_.insert(group_id);
    } else {
        currently_highlighted_groups_.remove(group_id);
    }
    
    // Broadcast to all plots except the requesting one
    emit groupHighlightRequested(group_id, highlight);
}

void GroupCoordinator::connectToGroupManager()
{
    if (!group_manager_) {
        return;
    }

    // Connect to actual GroupManager signals
    connect(group_manager_, &GroupManager::groupCreated,
            this, [this](int group_id) {
                // Get group details from GroupManager
                auto group = group_manager_->getGroup(group_id);
                if (group) {
                    QString name = group->name;
                    QColor color = group->color;
                    onGroupCreated(group_id, name, color);
                }
            });

    connect(group_manager_, &GroupManager::groupRemoved,
            this, &GroupCoordinator::onGroupRemoved);

    connect(group_manager_, &GroupManager::groupModified,
            this, &GroupCoordinator::onGroupPropertiesChanged);

    connect(group_manager_, &GroupManager::pointAssignmentsChanged,
            this, [this](const std::unordered_set<int>& affected_groups) {
                // When point assignments change, notify all plots about the affected groups
                for (int group_id : affected_groups) {
                    emit groupPropertiesChanged(group_id);
                }
                qDebug() << "GroupCoordinator: Point assignments changed for" << affected_groups.size() << "groups";
            });
    
    qDebug() << "GroupCoordinator::connectToGroupManager: Connected to GroupManager signals";
}

void GroupCoordinator::disconnectFromGroupManager()
{
    if (!group_manager_) {
        return;
    }

    // Disconnect all GroupManager signals
    disconnect(group_manager_, nullptr, this, nullptr);
    qDebug() << "GroupCoordinator::disconnectFromGroupManager: Disconnected from GroupManager";
}

void GroupCoordinator::connectToPlot(AbstractPlotWidget* plot_widget)
{
    if (!plot_widget) {
        return;
    }

    // Connect to plot widget signals for group interactions
    connect(plot_widget, &AbstractPlotWidget::groupHighlightRequested,
            this, [this, plot_widget](int group_id, bool highlight) {
                // Find the plot_id for this widget
                QString plot_id;
                for (auto it = registered_plots_.begin(); it != registered_plots_.end(); ++it) {
                    if (it.value() == plot_widget) {
                        plot_id = it.key();
                        break;
                    }
                }
                onGroupHighlightRequested(group_id, highlight, plot_id);
            });

    // Connect our signals to the plot's slots for group coordination
    connect(this, &GroupCoordinator::groupSelectionChanged,
            plot_widget, &AbstractPlotWidget::onGroupSelectionChanged);

    connect(this, &GroupCoordinator::groupHighlightRequested,
            plot_widget, &AbstractPlotWidget::onGroupHighlightChanged);

    connect(this, &GroupCoordinator::groupCreated,
            plot_widget, &AbstractPlotWidget::onGroupCreated);

    connect(this, &GroupCoordinator::groupRemoved,
            plot_widget, &AbstractPlotWidget::onGroupRemoved);
    
    qDebug() << "GroupCoordinator::connectToPlot: Connected to plot signals";
}

void GroupCoordinator::disconnectFromPlot(AbstractPlotWidget* plot_widget)
{
    if (!plot_widget) {
        return;
    }

    // Disconnect all signals from this plot widget
    disconnect(plot_widget, nullptr, this, nullptr);
    qDebug() << "GroupCoordinator::disconnectFromPlot: Disconnected from plot signals";
}

void GroupCoordinator::handleGroupManagerSignals()
{
    // This slot is available for general GroupManager signal handling
    // Currently, specific handlers are used in connectToGroupManager()
    qDebug() << "GroupCoordinator::handleGroupManagerSignals: Generic signal handler called";
}

void GroupCoordinator::broadcastToPlots(const QString& event_type, int group_id, const QString& exclude_plot_id)
{
    qDebug() << "GroupCoordinator::broadcastToPlots:" << event_type << "for group" << group_id 
             << "excluding plot:" << exclude_plot_id;
    
    for (auto it = registered_plots_.begin(); it != registered_plots_.end(); ++it) {
        const QString& plot_id = it.key();
        AbstractPlotWidget* plot = it.value();
        
        // Skip the requesting plot to avoid echo
        if (plot_id == exclude_plot_id) {
            continue;
        }
        
        if (plot) {
            // Send the event to this plot
            // The actual implementation will depend on the plot interface
            qDebug() << "  Broadcasting" << event_type << "to plot:" << plot_id;
        }
    }
}
