#include "ToolboxPanel.hpp"
#include "ui_ToolboxPanel.h"
#include "DraggableListWidget.hpp"
#include "../Groups/GroupManagementWidget.hpp"
#include "../Groups/GroupManager.hpp"
#include "../Tables/TableManager.hpp"
#include "../Tables/TableDesignerWidget.hpp"
#include "DataManager/DataManager.hpp"

#include <QListWidget>
#include <QListWidgetItem>
#include <QVBoxLayout>
#include <QIcon>
#include <QDebug>

ToolboxPanel::ToolboxPanel(GroupManager* group_manager, std::shared_ptr<DataManager> data_manager, QWidget* parent)
    : QWidget(parent),
      ui(new Ui::ToolboxPanel),
      _group_widget(nullptr),
      _table_manager(std::make_unique<TableManager>(data_manager, this)),
      _table_designer_widget(nullptr) {
    ui->setupUi(this);
    
    // Create and add the group management widget at the top
    _group_widget = new GroupManagementWidget(group_manager, this);
    
    // Create the table designer widget
    _table_designer_widget = new TableDesignerWidget(_table_manager.get(), data_manager, this);
    
    // Insert widgets into the layout
    auto * layout = ui->verticalLayout;
    layout->insertWidget(0, _group_widget);  // Insert at index 0 (top)
    
    // Replace the standard QListWidget with our custom DraggableListWidget
    DraggableListWidget* draggable_list = new DraggableListWidget(this);
    
    // Remove the old list widget and add the new one
    delete ui->plot_list;
    layout->addWidget(draggable_list);
    ui->plot_list = draggable_list;
    
    // Add the table designer widget after the plot list
    layout->addWidget(_table_designer_widget);
    
    // Initialize with available plot types
    initializeToolbox();
    
    // Connect signals
    connect(ui->plot_list, &QListWidget::itemDoubleClicked,
            this, &ToolboxPanel::handleItemDoubleClicked);
}

ToolboxPanel::~ToolboxPanel() {
    delete ui;
}

void ToolboxPanel::handleItemDoubleClicked(QListWidgetItem* item) {
    // For now, just show that double-click was detected
    // This could be used for alternative creation methods
    QString plot_type = item->data(Qt::UserRole).toString();
    qDebug() << "Double-clicked plot type:" << plot_type;
}

void ToolboxPanel::initializeToolbox() {
    // Add available plot types
    addPlotType("scatter_plot", "Scatter Plot", ":/icons/scatter_plot.png");
    addPlotType("spatial_overlay_plot", "Spatial Overlay Plot", ":/icons/spatial_overlay.png");
    addPlotType("event_plot", "Event Plot", ":/icons/event_plot.png");
    
    // Placeholder for future plot types
    // addPlotType("line_plot", "Line Plot", ":/icons/line_plot.png");
    // addPlotType("histogram", "Histogram", ":/icons/histogram.png");
    // addPlotType("box_plot", "Box Plot", ":/icons/box_plot.png");
}

void ToolboxPanel::addPlotType(const QString& plot_type, const QString& display_name, const QString& icon_path) {
    QListWidgetItem* item = new QListWidgetItem(display_name);
    
    // Store the plot type identifier in the item's user data
    item->setData(Qt::UserRole, plot_type);
    
    // Set icon if provided and exists
    if (!icon_path.isEmpty()) {
        QIcon icon(icon_path);
        if (!icon.isNull()) {
            item->setIcon(icon);
        }
    }
    
    // Set tooltip
    item->setToolTip(QString("Drag to create a new %1").arg(display_name));
    
    ui->plot_list->addItem(item);
} 