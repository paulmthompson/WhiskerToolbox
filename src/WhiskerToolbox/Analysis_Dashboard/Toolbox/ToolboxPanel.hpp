#ifndef TOOLBOXPANEL_HPP
#define TOOLBOXPANEL_HPP

#include <QWidget>

namespace Ui {
class ToolboxPanel;
}

class QListWidget;
class QListWidgetItem;

/**
 * @brief Toolbox panel containing available plot types for dragging to the dashboard
 * 
 * This widget provides a list of available plot types that users can drag
 * into the main dashboard graphics view to create new plots.
 */
class ToolboxPanel : public QWidget {
    Q_OBJECT

public:
    explicit ToolboxPanel(QWidget* parent = nullptr);
    ~ToolboxPanel() override;

private slots:
    /**
     * @brief Handle when user starts dragging an item from the toolbox
     * @param item The list widget item being dragged
     */
    void handleItemDoubleClicked(QListWidgetItem* item);

private:
    Ui::ToolboxPanel* ui;

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