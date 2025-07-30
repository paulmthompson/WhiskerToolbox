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
class TableManager;
class TableDesignerWidget;
class DataManager;

/**
 * @brief Toolbox panel containing available plot types for dragging to the dashboard
 * 
 * This widget provides a list of available plot types that users can drag
 * into the main dashboard graphics view to create new plots. It also contains
 * the groups management interface and table designer widget.
 */
class ToolboxPanel : public QWidget {
    Q_OBJECT

public:
    explicit ToolboxPanel(GroupManager* group_manager, std::shared_ptr<DataManager> data_manager, QWidget* parent = nullptr);
    ~ToolboxPanel() override;

    /**
     * @brief Get the table manager instance
     * @return Pointer to the table manager
     */
    TableManager* getTableManager() const { return _table_manager.get(); }

private slots:
    /**
     * @brief Handle when user starts dragging an item from the toolbox
     * @param item The list widget item being dragged
     */
    void handleItemDoubleClicked(QListWidgetItem* item);

private:
    Ui::ToolboxPanel* ui;
    GroupManagementWidget* _group_widget;
    std::unique_ptr<TableManager> _table_manager;
    TableDesignerWidget* _table_designer_widget;

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
    void addPlotType(const QString& plot_type, const QString& display_name, const QString& icon_path = QString());
};

#endif // TOOLBOXPANEL_HPP 