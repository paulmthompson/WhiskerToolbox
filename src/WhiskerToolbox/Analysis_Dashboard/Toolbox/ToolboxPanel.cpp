#include "ToolboxPanel.hpp"
#include "../Groups/GroupManagementWidget.hpp"
#include "../Groups/GroupManager.hpp"
#include "../Tables/TableDesignerWidget.hpp"
#include "../Tables/TableManager.hpp"
#include "DataManager/DataManager.hpp"
#include "ui_ToolboxPanel.h"

#include <QDebug>
#include <QIcon>
#include <QListWidget>
#include <QListWidgetItem>
#include <QPushButton>
#include <QVBoxLayout>

ToolboxPanel::ToolboxPanel(GroupManager * group_manager, std::shared_ptr<DataManager> data_manager, QWidget * parent)
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
    layout->insertWidget(0, _group_widget);// Insert at index 0 (top)

    // Add the table designer widget after the plot list
    layout->addWidget(_table_designer_widget);

    // Initialize with available plot types
    initializeToolbox();

    // Connect signals
    connect(ui->plot_list, &QListWidget::itemSelectionChanged,
            this, [this]() {
                // Enable/disable the Add button based on selection
                ui->add_button->setEnabled(ui->plot_list->currentItem() != nullptr);
            });

    connect(ui->plot_list, &QListWidget::itemDoubleClicked,
            this, &ToolboxPanel::handleItemDoubleClicked);

    connect(ui->add_button, &QPushButton::clicked,
            this, &ToolboxPanel::handleAddButtonClicked);
}

ToolboxPanel::~ToolboxPanel() {
    delete ui;
}

void ToolboxPanel::handleAddButtonClicked() {
    QListWidgetItem * current_item = ui->plot_list->currentItem();
    if (current_item) {
        QString plot_type = current_item->data(Qt::UserRole).toString();
        if (!plot_type.isEmpty()) {
            emit plotTypeSelected(plot_type);
        }
    }
}

void ToolboxPanel::handleItemDoubleClicked(QListWidgetItem * item) {
    // Double-click also adds the plot
    QString plot_type = item->data(Qt::UserRole).toString();
    if (!plot_type.isEmpty()) {
        emit plotTypeSelected(plot_type);
    }
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

void ToolboxPanel::addPlotType(QString const & plot_type, QString const & display_name, QString const & icon_path) {
    QListWidgetItem * item = new QListWidgetItem(display_name);

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
    item->setToolTip(QString("Double-click or select and click 'Add' to create a new %1").arg(display_name));

    ui->plot_list->addItem(item);
}