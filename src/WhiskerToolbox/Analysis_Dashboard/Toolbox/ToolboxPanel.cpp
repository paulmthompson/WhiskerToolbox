#include "ToolboxPanel.hpp"
#include "ui_ToolboxPanel.h"
#include "DraggableListWidget.hpp"

#include <QListWidget>
#include <QListWidgetItem>
#include <QVBoxLayout>
#include <QIcon>
#include <QDebug>

ToolboxPanel::ToolboxPanel(QWidget* parent)
    : QWidget(parent),
      ui(new Ui::ToolboxPanel) {
    ui->setupUi(this);
    
    // Replace the standard QListWidget with our custom DraggableListWidget
    DraggableListWidget* draggable_list = new DraggableListWidget(this);
    
    // Remove the old list widget and add the new one
    QLayout* layout = ui->verticalLayout;
    delete ui->plot_list;
    layout->addWidget(draggable_list);
    ui->plot_list = draggable_list;
    
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