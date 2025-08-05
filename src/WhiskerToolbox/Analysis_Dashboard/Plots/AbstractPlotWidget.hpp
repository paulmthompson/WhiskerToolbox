#ifndef ABSTRACTPLOTWIDGET_HPP
#define ABSTRACTPLOTWIDGET_HPP

#include "AbstractPlotParameters.hpp"

#include <QGraphicsWidget>
#include <QString>
#include <QStringList>

#include <memory>

class DataManager;
class DataSourceRegistry;
class GroupManager;
class TableManager;
class TableView;
class QGraphicsSceneMouseEvent;

/**
 * @brief Abstract base class for all plot widgets in the Analysis Dashboard
 * 
 * This class provides the common interface and functionality for plot widgets
 * that can be displayed in the dashboard's graphics scene. Plot widgets can
 * use Qt graphics infrastructure or OpenGL for rendering.
 */
class AbstractPlotWidget : public QGraphicsWidget {
    Q_OBJECT

public:
    explicit AbstractPlotWidget(QGraphicsItem * parent = nullptr);
    ~AbstractPlotWidget() override = default;

    /**
     * @brief Get the plot type name (e.g., "Scatter Plot", "Line Plot")
     * @return The human-readable name of this plot type
     */
    virtual QString getPlotType() const = 0;

    /**
     * @brief Get the plot instance name/title
     * @return The specific name/title for this plot instance
     */
    virtual QString getPlotTitle() const;

    /**
     * @brief Set the plot instance name/title
     * @param title The new title for this plot
     */
    virtual void setPlotTitle(QString const & title);

    /**
     * @brief Set the data source registry for unified data access
     * @param data_source_registry Pointer to the data source registry
     */
    virtual void setDataSourceRegistry(DataSourceRegistry* data_source_registry);

    /**
     * @brief Set the group manager for data grouping
     * @param group_manager Pointer to the group manager
     */
    virtual void setGroupManager(GroupManager * group_manager);

    /**
     * @brief Get the unique identifier for this plot instance
     * @return Unique ID string
     */
    QString getPlotId() const;

signals:
    /**
     * @brief Emitted when this plot is selected/clicked
     * @param plot_id The unique ID of this plot
     */
    void plotSelected(QString const & plot_id);

    /**
     * @brief Emitted when plot properties change and need to be updated in properties panel
     * @param plot_id The unique ID of this plot
     */
    void propertiesChanged(QString const & plot_id);

    /**
     * @brief Emitted when this plot needs to be re-rendered/updated
     * @param plot_id The unique ID of this plot
     */
    void renderUpdateRequested(QString const & plot_id);

    /**
     * @brief Emitted when user requests to jump to a specific frame
     * @param time_frame_index The time frame index to jump to
     */
    void frameJumpRequested(int64_t time_frame_index, std::string const & data_key);

    /**
     * @brief Emitted when this plot requests a group to be highlighted across all plots
     * @param group_id The group ID to highlight
     * @param highlight True to highlight, false to unhighlight
     * @param plot_id The ID of this plot (to avoid echo)
     */
    void groupHighlightRequested(int group_id, bool highlight, QString const & plot_id);

public slots:
    /**
     * @brief Handle group selection changes from GroupCoordinator
     * @param group_id The group ID that was selected/deselected
     * @param selected True if selected, false if deselected
     */
    virtual void onGroupSelectionChanged(int group_id, bool selected) {}

    /**
     * @brief Handle group highlighting requests from GroupCoordinator
     * @param group_id The group ID to highlight/unhighlight
     * @param highlight True to highlight, false to unhighlight
     */
    virtual void onGroupHighlightChanged(int group_id, bool highlight) {}

    /**
     * @brief Handle new group creation from GroupCoordinator
     * @param group_id The new group ID
     * @param group_name The group name
     * @param group_color The group color
     */
    virtual void onGroupCreated(int group_id, QString const & group_name, QColor const & group_color) {}

    /**
     * @brief Handle group removal from GroupCoordinator
     * @param group_id The removed group ID
     */
    virtual void onGroupRemoved(int group_id) {}

protected:
    /**
     * @brief Handle mouse press events for selection
     */
    void mousePressEvent(QGraphicsSceneMouseEvent * event) override;

    /**
     * @brief Get the parameters struct for this plot
     * @return Reference to the parameters struct
     */
    AbstractPlotParameters & getParameters();

    /**
     * @brief Get the parameters struct for this plot (const version)
     * @return Const reference to the parameters struct
     */
    AbstractPlotParameters const & getParameters() const;

protected:
    AbstractPlotParameters _parameters;
};

#endif// ABSTRACTPLOTWIDGET_HPP