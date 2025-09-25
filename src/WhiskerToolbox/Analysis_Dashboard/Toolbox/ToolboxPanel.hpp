#ifndef TOOLBOXPANEL_HPP
#define TOOLBOXPANEL_HPP

#include <QWidget>
#include <memory>

namespace Ui {
class ToolboxPanel;
}

class QListWidget;
class QListWidgetItem;

/**
 * @brief Toolbox panel containing available plot types for adding to the dashboard
 * 
 * This widget provides a list of available plot types that users can select
 * and add to the main dashboard graphics view to create new plots.
 */
class ToolboxPanel : public QWidget {
    Q_OBJECT

public:
    explicit ToolboxPanel(QWidget * parent = nullptr);
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
    // TableDesignerWidget removed from dashboard toolbox
    void* _table_designer_widget; // legacy placeholder, not used

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

    /**
     * @brief Resize the plot list widget to fit its contents
     */
    void resizeListToContents();
};

#endif// TOOLBOXPANEL_HPP