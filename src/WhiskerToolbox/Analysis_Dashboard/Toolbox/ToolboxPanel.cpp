#include "ToolboxPanel.hpp"
#include "ui_ToolboxPanel.h"

#include <QDebug>
#include <QIcon>
#include <QListWidget>
#include <QListWidgetItem>
#include <QPushButton>
#include <QVBoxLayout>

ToolboxPanel::ToolboxPanel(QWidget * parent)
    : QWidget(parent),
      ui(new Ui::ToolboxPanel),
      _table_designer_widget(nullptr) {
    ui->setupUi(this);

    // Set size policy to minimize vertical space usage
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Minimum);

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

    // Resize the list widget to fit its contents
    resizeListToContents();
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

void ToolboxPanel::resizeListToContents() {
    if (!ui->plot_list) return;

    int item_count = ui->plot_list->count();
    if (item_count == 0) return;

    // Calculate the height needed for all items
    int item_height = ui->plot_list->sizeHintForRow(0);
    if (item_height <= 0) {
        // Fallback height if sizeHintForRow doesn't work
        item_height = 25; // Default item height including spacing
    }

    // Add spacing between items
    int spacing = ui->plot_list->spacing();
    int total_height = (item_height * item_count) + (spacing * (item_count - 1));

    // Add some padding for the list widget frame/scrollbar area
    total_height += 10;

    // Set the fixed height for the list widget
    ui->plot_list->setFixedHeight(total_height);
    ui->plot_list->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
}
