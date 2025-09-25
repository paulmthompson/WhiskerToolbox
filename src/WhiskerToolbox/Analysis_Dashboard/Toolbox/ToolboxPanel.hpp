#ifndef TOOLBOXPANEL_HPP
#define TOOLBOXPANEL_HPP

#include <QWidget>

#include <memory>

namespace Ui {
class ToolboxPanel;
}

class QListWidget;
class QListWidgetItem;
class GroupManager;
class GroupManagementWidget;
class DataManager;

/**
 * @brief Toolbox panel containing available plot types for adding to the dashboard
 * 
 * This widget provides a list of available plot types that users can select
 * and add to the main dashboard graphics view to create new plots. It also contains
 * the groups management interface and table designer widget.
 */
class ToolboxPanel : public QWidget {
    Q_OBJECT

public:
    explicit ToolboxPanel(GroupManager * group_manager, std::shared_ptr<DataManager> data_manager, QWidget * parent = nullptr);
    ~ToolboxPanel() override;


signals:
    /**
     * @brief Emitted when user wants to add a plot of the specified type
     * @param plot_type The type of plot to add
     */
    void plotTypeSelected(QString const & plot_type);

private slots:
    /**
     * @brief Handle when user clicks the Add button
     */
    void handleAddButtonClicked();

    /**
     * @brief Handle when user double-clicks an item in the list
     * @param item The list widget item that was double-clicked
     */
    void handleItemDoubleClicked(QListWidgetItem * item);

private:
    Ui::ToolboxPanel * ui;
    GroupManagementWidget * _group_widget;
    // TableDesignerWidget removed from dashboard toolbox
    void * _table_designer_widget;// legacy placeholder, not used

    /**
     * @brief Initialize the toolbox with available plot types
     */
    void initializeToolbox();

    /**
     * @brief Create a list item for a plot type
     * @param plot_type The internal plot type identifier
     * @param display_name The human-readable display name
     * @param icon_path Path to the icon for this plot type (optional)
     */
    void addPlotType(QString const & plot_type, QString const & display_name, QString const & icon_path = QString());
};

#endif// TOOLBOXPANEL_HPP