#include "3DPlotPropertiesWidget.hpp"

#include "Core/3DPlotState.hpp"
#include "DataManager/DataManager.hpp"
#include "DataManager/Points/Point_Data.hpp"
#include "UI/3DPlotWidget.hpp"

#include "ui_3DPlotPropertiesWidget.h"

#include <QTableWidgetItem>

#include <algorithm>
#include <set>

ThreeDPlotPropertiesWidget::ThreeDPlotPropertiesWidget(std::shared_ptr<ThreeDPlotState> state,
                                                       std::shared_ptr<DataManager> data_manager,
                                                       QWidget * parent)
    : QWidget(parent),
      ui(new Ui::ThreeDPlotPropertiesWidget),
      _state(state),
      _data_manager(data_manager),
      _plot_widget(nullptr),
      _dm_observer_id(-1) {
    ui->setupUi(this);

    // Set up plot data table
    ui->plot_data_table->setColumnCount(1);
    ui->plot_data_table->setHorizontalHeaderLabels(QStringList() << "Data Key");
    ui->plot_data_table->horizontalHeader()->setStretchLastSection(true);
    ui->plot_data_table->setSelectionBehavior(QAbstractItemView::SelectRows);
    ui->plot_data_table->setSelectionMode(QAbstractItemView::SingleSelection);
    ui->plot_data_table->verticalHeader()->setVisible(false);
    ui->plot_data_table->setShowGrid(true);

    // Populate combo box
    _populateAddDataComboBox();

    // Set up DataManager observer to refresh combo box when data changes
    if (_data_manager) {
        _dm_observer_id = _data_manager->addObserver([this]() {
            _populateAddDataComboBox();
        });
    }

    // Connect UI signals to slots
    connect(ui->add_data_button, &QPushButton::clicked,
            this, &ThreeDPlotPropertiesWidget::_onAddDataClicked);
    connect(ui->remove_data_button, &QPushButton::clicked,
            this, &ThreeDPlotPropertiesWidget::_onRemoveDataClicked);
    connect(ui->plot_data_table, &QTableWidget::itemSelectionChanged,
            this, &ThreeDPlotPropertiesWidget::_onPlotDataSelectionChanged);

    // Connect state signals
    if (_state) {
        connect(_state.get(), &ThreeDPlotState::plotDataKeyAdded,
                this, &ThreeDPlotPropertiesWidget::_onStatePlotDataKeyAdded);
        connect(_state.get(), &ThreeDPlotState::plotDataKeyRemoved,
                this, &ThreeDPlotPropertiesWidget::_onStatePlotDataKeyRemoved);

        // Initialize UI from state
        _updateUIFromState();
    }
}

void ThreeDPlotPropertiesWidget::setPlotWidget(ThreeDPlotWidget * plot_widget) {
    _plot_widget = plot_widget;

    if (!_plot_widget) {
        return;
    }

    // Connect plot widget controls if needed
    // Widget is empty for now
}

ThreeDPlotPropertiesWidget::~ThreeDPlotPropertiesWidget() {
    // Remove DataManager observer callback
    if (_data_manager && _dm_observer_id != -1) {
        _data_manager->removeObserver(_dm_observer_id);
    }
    delete ui;
}

void ThreeDPlotPropertiesWidget::_populateAddDataComboBox() {
    ui->add_data_combo->clear();

    if (!_data_manager) {
        return;
    }

    // Get all PointData keys
    auto point_keys = _data_manager->getKeys<PointData>();

    if (point_keys.empty()) {
        return;
    }

    // Sort keys before adding to combo box
    std::sort(point_keys.begin(), point_keys.end());

    // Get currently added keys to filter them out
    std::set<std::string> added_keys;
    if (_state) {
        auto added = _state->getPlotDataKeys();
        for (QString const & key : added) {
            added_keys.insert(key.toStdString());
        }
    }

    // Add PointData keys that aren't already added
    for (auto const & key: point_keys) {
        if (added_keys.find(key) == added_keys.end()) {
            QString key_str = QString::fromStdString(key);
            ui->add_data_combo->addItem(key_str, key_str);
        }
    }
}

void ThreeDPlotPropertiesWidget::_onAddDataClicked() {
    if (!_state || !_data_manager) {
        return;
    }

    QString data_key = ui->add_data_combo->currentData().toString();
    if (data_key.isEmpty()) {
        return;
    }

    _state->addPlotDataKey(data_key);
}

void ThreeDPlotPropertiesWidget::_onRemoveDataClicked() {
    if (!_state) {
        return;
    }

    QList<QTableWidgetItem *> selected = ui->plot_data_table->selectedItems();
    if (selected.isEmpty()) {
        return;
    }

    int row = selected.first()->row();
    QTableWidgetItem * key_item = ui->plot_data_table->item(row, 0);
    if (key_item) {
        QString data_key = key_item->text();
        _state->removePlotDataKey(data_key);
    }
}

void ThreeDPlotPropertiesWidget::_onPlotDataSelectionChanged() {
    QList<QTableWidgetItem *> selected = ui->plot_data_table->selectedItems();
    bool has_selection = !selected.isEmpty();
    ui->remove_data_button->setEnabled(has_selection);
}

void ThreeDPlotPropertiesWidget::_onStatePlotDataKeyAdded(QString const & data_key) {
    Q_UNUSED(data_key)
    _updatePlotDataTable();
    _populateAddDataComboBox();
}

void ThreeDPlotPropertiesWidget::_onStatePlotDataKeyRemoved(QString const & data_key) {
    Q_UNUSED(data_key)
    _updatePlotDataTable();
    _populateAddDataComboBox();
    // Clear selection if the removed key was selected
    ui->plot_data_table->clearSelection();
    ui->remove_data_button->setEnabled(false);
}

void ThreeDPlotPropertiesWidget::_updatePlotDataTable() {
    if (!_state) {
        return;
    }

    ui->plot_data_table->setRowCount(0);

    auto data_keys = _state->getPlotDataKeys();
    for (QString const & data_key: data_keys) {
        int row = ui->plot_data_table->rowCount();
        ui->plot_data_table->insertRow(row);

        QTableWidgetItem * key_item = new QTableWidgetItem(data_key);
        key_item->setFlags(key_item->flags() & ~Qt::ItemIsEditable);
        ui->plot_data_table->setItem(row, 0, key_item);
    }

    // Resize table to fit content dynamically
    ui->plot_data_table->resizeRowsToContents();

    int row_count = ui->plot_data_table->rowCount();
    if (row_count == 0) {
        // If no rows, set minimum height to just show header
        ui->plot_data_table->setMinimumHeight(ui->plot_data_table->horizontalHeader()->height());
        ui->plot_data_table->setMaximumHeight(ui->plot_data_table->horizontalHeader()->height());
    } else {
        // Calculate height: header + (row height * row count)
        int header_height = ui->plot_data_table->horizontalHeader()->height();
        int row_height = ui->plot_data_table->rowHeight(0);
        int total_height = header_height + (row_height * row_count);

        ui->plot_data_table->setMinimumHeight(total_height);
        ui->plot_data_table->setMaximumHeight(total_height);
    }
}

void ThreeDPlotPropertiesWidget::_updateUIFromState() {
    if (!_state) {
        return;
    }

    // Update plot data table from state
    _updatePlotDataTable();
    _populateAddDataComboBox();
}
