#include "TableViewerWidget.hpp"
#include "ui_TableViewerWidget.h"

#include "DataManager/utils/TableView/core/TableView.h"
#include "DataManager/utils/TableView/interfaces/IRowSelector.h"
#include "PaginatedTableModel.hpp"

#include <QHeaderView>
#include <QMessageBox>
#include <QScrollBar>

TableViewerWidget::TableViewerWidget(QWidget * parent)
    : QWidget(parent),
      ui(new Ui::TableViewerWidget),
      _model(new PaginatedTableModel(this)) {

    ui->setupUi(this);

    // Set up the table view
    ui->table_view->setModel(_model);
    ui->table_view->horizontalHeader()->setStretchLastSection(true);
    ui->table_view->horizontalHeader()->setSectionsMovable(true);
    ui->table_view->verticalHeader()->setDefaultSectionSize(25);

    _model->setPageSize(50);

    connectSignals();

    // Track column order when user reorders columns
    connect(ui->table_view->horizontalHeader(), &QHeaderView::sectionMoved, this,
            [this](int /*logicalIndex*/, int /*oldVisualIndex*/, int /*newVisualIndex*/) {
                // Reconstruct current order by visual indices
                QStringList order;
                int cols = _model ? _model->columnCount(QModelIndex()) : 0;
                for (int v = 0; v < cols; ++v) {
                    int logical = ui->table_view->horizontalHeader()->logicalIndex(v);
                    QVariant name = _model->headerData(logical, Qt::Horizontal, Qt::DisplayRole);
                    order.push_back(name.toString());
                }
                _currentColumnOrder = order;
                emit columnsReordered(order);
            });
}

TableViewerWidget::~TableViewerWidget() {
    delete ui;
}

void TableViewerWidget::setTableView(std::shared_ptr<TableView> table_view, QString const & table_name) {
    _table_name = table_name.isEmpty() ? "Unnamed Table" : table_name;

    if (table_view) {
        _total_rows = table_view->getRowCount();
        _model->setTableView(table_view);

        // Update UI
        ui->table_name_label->setText(QString("Table: %1").arg(_table_name));
        ui->row_count_label->setText(QString("Rows: %1").arg(_total_rows));
        ui->column_count_label->setText(QString("Columns: %1").arg(table_view->getColumnCount()));
        // Navigation hidden; skip controls
        // Initialize current column order from table view
        _currentColumnOrder.clear();
        for (auto const & name: table_view->getColumnNames()) {
            _currentColumnOrder.push_back(QString::fromStdString(name));
        }

    } else {
        clearTable();
    }
}

void TableViewerWidget::setTableConfiguration(std::unique_ptr<IRowSelector> row_selector,
                                              std::vector<ColumnInfo> column_infos,
                                              std::shared_ptr<DataManager> data_manager,
                                              QString const & table_name,
                                              QString const & row_source) {
    _table_name = table_name.isEmpty() ? "Unnamed Table" : table_name;

    if (row_selector && data_manager) {
        _total_rows = row_selector->getRowCount();
        QStringList desiredOrder = _currentColumnOrder;
        _model->setSourceTable(std::move(row_selector), std::move(column_infos), data_manager, row_source);

        // Update UI
        ui->table_name_label->setText(QString("Table: %1").arg(_table_name));
        ui->row_count_label->setText(QString("Rows: %1").arg(_total_rows));
        ui->column_count_label->setText(QString("Columns: %1").arg(column_infos.size()));
        // Navigation hidden; skip controls
        // Initialize or reapply column order
        if (!desiredOrder.isEmpty()) {
            // Reorder visual columns to match previous order
            int cols = _model->columnCount(QModelIndex());
            std::map<QString, int> nameToIndex;
            for (int c = 0; c < cols; ++c) {
                nameToIndex[_model->headerData(c, Qt::Horizontal, Qt::DisplayRole).toString()] = c;
            }
            int visualPos = 0;
            for (auto const & name: desiredOrder) {
                auto it = nameToIndex.find(name);
                if (it == nameToIndex.end()) continue;
                int logical = it->second;
                int currentVisual = ui->table_view->horizontalHeader()->visualIndex(logical);
                if (currentVisual != visualPos) {
                    ui->table_view->horizontalHeader()->moveSection(currentVisual, visualPos);
                }
                ++visualPos;
            }
            _currentColumnOrder = desiredOrder;
        } else {
            _currentColumnOrder.clear();
            int cols = _model->columnCount(QModelIndex());
            for (int c = 0; c < cols; ++c) {
                _currentColumnOrder.push_back(_model->headerData(c, Qt::Horizontal, Qt::DisplayRole).toString());
            }
        }

    } else {
        clearTable();
    }
}

void TableViewerWidget::clearTable() {
    _table_name.clear();
    _total_rows = 0;
    _model->clearTable();

    // Update UI
    ui->table_name_label->setText("Table: (None)");
    ui->row_count_label->setText("Rows: 0");
    ui->column_count_label->setText("Columns: 0");
}

void TableViewerWidget::setPageSize(size_t page_size) {
    _model->setPageSize(page_size);
}

QString TableViewerWidget::getTableName() const {
    return _table_name;
}

bool TableViewerWidget::hasTable() const {
    return _total_rows > 0;
}

void TableViewerWidget::onTableScrolled() {

    // Emit signal with current visible row
    QModelIndex top_left = ui->table_view->indexAt(ui->table_view->rect().topLeft());
    if (top_left.isValid()) {
        emit rowScrolled(static_cast<size_t>(top_left.row()));
    }
}

void TableViewerWidget::connectSignals() {

    // Table scroll events
    connect(ui->table_view->verticalScrollBar(), &QScrollBar::valueChanged,
            this, &TableViewerWidget::onTableScrolled);
    connect(ui->table_view->horizontalScrollBar(), &QScrollBar::valueChanged,
            this, &TableViewerWidget::onTableScrolled);
}
