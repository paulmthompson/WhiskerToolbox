#include "TableViewerWidget.hpp"
#include "ui_TableViewerWidget.h"

#include "PaginatedTableModel.hpp"
#include "DataManager/utils/TableView/interfaces/IRowSelector.h"

#include <QScrollBar>
#include <QMessageBox>
#include <QHeaderView>

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
    
    // Set default page size to 1000 (matches the combo box default)
    ui->page_size_combo->setCurrentText("1000");
    _model->setPageSize(1000);
    
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
        ui->goto_row_spinbox->setMaximum(static_cast<int>(std::max<size_t>(1, _total_rows)));
        ui->goto_row_spinbox->setValue(1);
        
        // Enable navigation controls
        ui->navigation_group->setEnabled(true);
        // Initialize current column order from table view
        _currentColumnOrder.clear();
        for (auto const & name : table_view->getColumnNames()) {
            _currentColumnOrder.push_back(QString::fromStdString(name));
        }
        
        updateRowInfo();
    } else {
        clearTable();
    }
}

void TableViewerWidget::setTableConfiguration(std::unique_ptr<IRowSelector> row_selector,
                                             std::vector<ColumnInfo> column_infos,
                                             std::shared_ptr<DataManager> data_manager,
                                             QString const & table_name) {
    _table_name = table_name.isEmpty() ? "Unnamed Table" : table_name;
    
    if (row_selector && data_manager) {
        _total_rows = row_selector->getRowCount();
        QStringList desiredOrder = _currentColumnOrder;
        _model->setSourceTable(std::move(row_selector), std::move(column_infos), data_manager);
        
        // Update UI
        ui->table_name_label->setText(QString("Table: %1").arg(_table_name));
        ui->row_count_label->setText(QString("Rows: %1").arg(_total_rows));
        ui->column_count_label->setText(QString("Columns: %1").arg(column_infos.size()));
        ui->goto_row_spinbox->setMaximum(static_cast<int>(std::max<size_t>(1, _total_rows)));
        ui->goto_row_spinbox->setValue(1);
        
        // Enable navigation controls
        ui->navigation_group->setEnabled(true);
        // Initialize or reapply column order
        if (!desiredOrder.isEmpty()) {
            // Reorder visual columns to match previous order
            int cols = _model->columnCount(QModelIndex());
            std::map<QString, int> nameToIndex;
            for (int c = 0; c < cols; ++c) {
                nameToIndex[_model->headerData(c, Qt::Horizontal, Qt::DisplayRole).toString()] = c;
            }
            int visualPos = 0;
            for (auto const & name : desiredOrder) {
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
        
        updateRowInfo();
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
    ui->goto_row_spinbox->setMaximum(1);
    ui->goto_row_spinbox->setValue(1);
    
    // Disable navigation controls
    ui->navigation_group->setEnabled(false);
    
    updateRowInfo();
}

void TableViewerWidget::setPageSize(size_t page_size) {
    _model->setPageSize(page_size);
    
    // Update combo box if it doesn't match
    QString page_size_str = QString::number(page_size);
    int index = ui->page_size_combo->findText(page_size_str);
    if (index >= 0) {
        ui->page_size_combo->setCurrentIndex(index);
    } else {
        // Add custom page size to combo box
        ui->page_size_combo->addItem(page_size_str);
        ui->page_size_combo->setCurrentText(page_size_str);
    }
}

QString TableViewerWidget::getTableName() const {
    return _table_name;
}

bool TableViewerWidget::hasTable() const {
    return _total_rows > 0;
}

void TableViewerWidget::onGoToRow() {
    if (_total_rows == 0) return;
    
    int target_row = ui->goto_row_spinbox->value() - 1; // Convert to 0-based
    target_row = std::max(0, std::min(target_row, static_cast<int>(_total_rows - 1)));
    
    // Scroll to the target row
    QModelIndex target_index = _model->index(target_row, 0);
    ui->table_view->scrollTo(target_index, QAbstractItemView::PositionAtTop);
    ui->table_view->selectRow(target_row);
    
    updateRowInfo();
    emit rowScrolled(static_cast<size_t>(target_row));
}

void TableViewerWidget::onPageSizeChanged() {
    bool ok;
    size_t new_page_size = ui->page_size_combo->currentText().toUInt(&ok);
    if (ok && new_page_size > 0) {
        _model->setPageSize(new_page_size);
    }
}

void TableViewerWidget::onTableScrolled() {
    updateRowInfo();
    
    // Emit signal with current visible row
    QModelIndex top_left = ui->table_view->indexAt(ui->table_view->rect().topLeft());
    if (top_left.isValid()) {
        emit rowScrolled(static_cast<size_t>(top_left.row()));
    }
}

void TableViewerWidget::updateRowInfo() {
    if (_total_rows == 0) {
        ui->current_row_label->setText("Current: No data");
        return;
    }
    
    // Get the currently visible top row
    QModelIndex top_left = ui->table_view->indexAt(ui->table_view->rect().topLeft());
    if (top_left.isValid()) {
        int current_row = top_left.row() + 1; // Convert to 1-based
        ui->current_row_label->setText(QString("Current: Row %1 of %2").arg(current_row).arg(_total_rows));
    } else {
        ui->current_row_label->setText(QString("Current: Row 1 of %1").arg(_total_rows));
    }
}

void TableViewerWidget::connectSignals() {
    // Navigation controls
    connect(ui->goto_row_button, &QPushButton::clicked, this, &TableViewerWidget::onGoToRow);
    connect(ui->goto_row_spinbox, QOverload<int>::of(&QSpinBox::valueChanged), 
            this, &TableViewerWidget::onGoToRow);
    connect(ui->page_size_combo, &QComboBox::currentTextChanged, 
            this, &TableViewerWidget::onPageSizeChanged);
    
    // Table scroll events
    connect(ui->table_view->verticalScrollBar(), &QScrollBar::valueChanged,
            this, &TableViewerWidget::onTableScrolled);
    connect(ui->table_view->horizontalScrollBar(), &QScrollBar::valueChanged,
            this, &TableViewerWidget::onTableScrolled);
}

#include "TableViewerWidget.moc"
