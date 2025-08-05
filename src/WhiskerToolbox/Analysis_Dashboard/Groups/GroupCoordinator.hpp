#ifndef GROUPCOORDINATOR_HPP
#define GROUPCOORDINATOR_HPP

#include <QObject>
#include <QString>
#include <QMap>
#include <QSet>

class GroupManager;
class AbstractPlotWidget;

/**
 * @brief Coordinates group-related events across multiple plot widgets
 * 
 * This class acts as a mediator between the GroupManager and plot widgets,
 * ensuring that group changes (creation, selection, highlighting) are 
 * synchronized across all plots in the dashboard.
 * 
 * For example, when a group is selected in a scatter plot, all other plots
 * (spatial overlay, event plots, etc.) will highlight the same group's data.
 */
class GroupCoordinator : public QObject {
    Q_OBJECT

public:
    explicit GroupCoordinator(GroupManager* group_manager, QObject* parent = nullptr);
    ~GroupCoordinator() override = default;

    /**
     * @brief Register a plot widget to receive group coordination events
     * @param plot_id The unique ID of the plot
     * @param plot_widget Pointer to the plot widget
     */
    void registerPlot(const QString& plot_id, AbstractPlotWidget* plot_widget);

    /**
     * @brief Unregister a plot widget from group coordination
     * @param plot_id The unique ID of the plot to unregister
     */
    void unregisterPlot(const QString& plot_id);

    /**
     * @brief Get the number of registered plots
     * @return Number of plots currently registered
     */
    int getRegisteredPlotCount() const { return registered_plots_.size(); }

    /**
     * @brief Check if a plot is registered
     * @param plot_id The plot ID to check
     * @return True if the plot is registered
     */
    bool isPlotRegistered(const QString& plot_id) const;

public slots:
    /**
     * @brief Handle group selection changes from GroupManager
     * @param group_id The ID of the group that was selected/deselected
     * @param selected True if group was selected, false if deselected
     */
    void onGroupSelectionChanged(int group_id, bool selected);

    /**
     * @brief Handle new group creation from GroupManager
     * @param group_id The ID of the newly created group
     * @param group_name The name of the new group
     * @param group_color The color assigned to the new group
     */
    void onGroupCreated(int group_id, const QString& group_name, const QColor& group_color);

    /**
     * @brief Handle group removal from GroupManager
     * @param group_id The ID of the group that was removed
     */
    void onGroupRemoved(int group_id);

    /**
     * @brief Handle group property changes (name, color, etc.)
     * @param group_id The ID of the group that was modified
     */
    void onGroupPropertiesChanged(int group_id);

    /**
     * @brief Handle group highlighting requests from plots
     * @param group_id The ID of the group to highlight
     * @param highlight True to highlight, false to unhighlight
     * @param requesting_plot_id ID of the plot making the request (to avoid echo)
     */
    void onGroupHighlightRequested(int group_id, bool highlight, const QString& requesting_plot_id);

signals:
    /**
     * @brief Emitted when a group's selection state changes
     * @param group_id The group ID
     * @param selected True if selected, false if deselected
     */
    void groupSelectionChanged(int group_id, bool selected);

    /**
     * @brief Emitted when a new group is created
     * @param group_id The new group ID
     * @param group_name The group name
     * @param group_color The group color
     */
    void groupCreated(int group_id, const QString& group_name, const QColor& group_color);

    /**
     * @brief Emitted when a group is removed
     * @param group_id The removed group ID
     */
    void groupRemoved(int group_id);

    /**
     * @brief Emitted when group properties change
     * @param group_id The group ID
     */
    void groupPropertiesChanged(int group_id);

    /**
     * @brief Emitted when a group should be highlighted across all plots
     * @param group_id The group ID to highlight
     * @param highlight True to highlight, false to unhighlight
     */
    void groupHighlightRequested(int group_id, bool highlight);

private slots:
    /**
     * @brief Internal handler for GroupManager signals
     */
    void handleGroupManagerSignals();

private:
    GroupManager* group_manager_;
    QMap<QString, AbstractPlotWidget*> registered_plots_;
    QSet<int> currently_selected_groups_;
    QSet<int> currently_highlighted_groups_;

    /**
     * @brief Connect to GroupManager signals
     */
    void connectToGroupManager();

    /**
     * @brief Disconnect from GroupManager signals
     */
    void disconnectFromGroupManager();

    /**
     * @brief Connect to a plot widget's group-related signals
     * @param plot_widget The plot widget to connect
     */
    void connectToPlot(AbstractPlotWidget* plot_widget);

    /**
     * @brief Disconnect from a plot widget's signals
     * @param plot_widget The plot widget to disconnect
     */
    void disconnectFromPlot(AbstractPlotWidget* plot_widget);

    /**
     * @brief Broadcast group event to all registered plots except the sender
     * @param event_type The type of group event
     * @param group_id The group ID
     * @param exclude_plot_id Plot ID to exclude from broadcast (sender)
     */
    void broadcastToPlots(const QString& event_type, int group_id, const QString& exclude_plot_id = QString());
};

#endif // GROUPCOORDINATOR_HPP
