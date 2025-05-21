#include "Line_Widget.hpp"
#include "ui_Line_Widget.h"

#include "DataManager.hpp"
#include "DataManager/Lines/Line_Data.hpp"
#include "LineTableModel.hpp"

#include <QTableView>
#include <iostream>

Line_Widget::Line_Widget(std::shared_ptr<DataManager> data_manager, QWidget * parent)
    : QWidget(parent),
      ui(new Ui::Line_Widget),
      _data_manager{std::move(data_manager)} {
    ui->setupUi(this);

    _line_table_model = new LineTableModel(this);
    ui->tableView->setModel(_line_table_model);
    ui->tableView->setSelectionBehavior(QAbstractItemView::SelectRows);
    ui->tableView->setEditTriggers(QAbstractItemView::NoEditTriggers);

    connect(ui->tableView, &QTableView::doubleClicked, this, &Line_Widget::_handleCellDoubleClicked);
    connect(ui->moveLineButton, &QPushButton::clicked, this, &Line_Widget::_moveLineButton_clicked);
    connect(ui->deleteLineButton, &QPushButton::clicked, this, &Line_Widget::_deleteLineButton_clicked);
}

Line_Widget::~Line_Widget() {
    delete ui;
}

void Line_Widget::openWidget() {
    this->show();
}

void Line_Widget::setActiveKey(std::string const & key) {
    if (_active_key == key && _callback_id != -1) {
        return;
    }

    removeCallbacks();

    _active_key = key;

    auto line_data = _data_manager->getData<LineData>(_active_key);
    if (line_data) {
        _line_table_model->setLines(line_data.get());
        _callback_id = _data_manager->addCallbackToData(_active_key, [this]() { _onDataChanged(); });
    } else {
        _line_table_model->setLines(nullptr);
        std::cerr << "Line_Widget: Could not find LineData with key: " << _active_key << std::endl;
    }
    updateTable();
    _populateMoveToComboBox();
}

void Line_Widget::removeCallbacks() {
    if (!_active_key.empty() && _callback_id != -1) {
        bool success = _data_manager->removeCallbackFromData(_active_key, _callback_id);
        if (success) {
            _callback_id = -1;
        } else {
            // Optionally log an error if callback removal failed, though it might not be critical
            // std::cerr << "Line_Widget: Failed to remove callback for key: " << _active_key << std::endl;
        }
    }
}

void Line_Widget::updateTable() {
    if (!_active_key.empty()) {
        auto line_data = _data_manager->getData<LineData>(_active_key);
        _line_table_model->setLines(line_data.get());
    } else {
        _line_table_model->setLines(nullptr);
    }
}

void Line_Widget::_handleCellDoubleClicked(QModelIndex const & index) {
    if (!index.isValid()) {
        return;
    }
    LineTableRow rowData = _line_table_model->getRowData(index.row());
    if (rowData.frame != -1) {
        emit frameSelected(rowData.frame);
    }
}

void Line_Widget::_onDataChanged() {
    updateTable();
}

void Line_Widget::_populateMoveToComboBox() {
    ui->moveToComboBox->clear();
    std::vector<std::string> line_keys = _data_manager->getKeys<LineData>();
    for (std::string const & key : line_keys) {
        if (key != _active_key) {
            ui->moveToComboBox->addItem(QString::fromStdString(key));
        }
    }
}

void Line_Widget::_moveLineButton_clicked() {
    QModelIndexList selectedIndexes = ui->tableView->selectionModel()->selectedIndexes();
    if (selectedIndexes.isEmpty()) {
        std::cout << "Line_Widget: No line selected to move." << std::endl;
        return;
    }
    int selected_row = selectedIndexes.first().row();
    LineTableRow row_data = _line_table_model->getRowData(selected_row);

    if (row_data.frame == -1) {
        std::cout << "Line_Widget: Selected row data is invalid." << std::endl;
        return;
    }

    QString target_key_qstr = ui->moveToComboBox->currentText();
    if (target_key_qstr.isEmpty()) {
        std::cout << "Line_Widget: No target selected in ComboBox." << std::endl;
        return;
    }
    std::string target_key = target_key_qstr.toStdString();

    auto source_line_data = _data_manager->getData<LineData>(_active_key);
    auto target_line_data = _data_manager->getData<LineData>(target_key);

    if (!source_line_data) {
        std::cerr << "Line_Widget: Source LineData object ('" << _active_key << "') not found." << std::endl;
        return;
    }
    if (!target_line_data) {
        std::cerr << "Line_Widget: Target LineData object ('" << target_key << "') not found." << std::endl;
        return;
    }

    std::vector<Line2D> const & lines_at_frame = source_line_data->getLinesAtTime(row_data.frame);
    if (row_data.lineIndex < 0 || static_cast<size_t>(row_data.lineIndex) >= lines_at_frame.size()) {
        std::cerr << "Line_Widget: Line index out of bounds for frame " << row_data.frame << std::endl;
        return;
    }
    Line2D line_to_move = lines_at_frame[row_data.lineIndex];

    target_line_data->addLineAtTime(row_data.frame, line_to_move);

    source_line_data->clearLineAtTime(row_data.frame, row_data.lineIndex);

    updateTable();

    _populateMoveToComboBox();

    std::cout << "Line moved from " << _active_key << " frame " << row_data.frame << " index " << row_data.lineIndex
              << " to " << target_key << std::endl;
}

void Line_Widget::_deleteLineButton_clicked() {
    QModelIndexList selectedIndexes = ui->tableView->selectionModel()->selectedIndexes();
    if (selectedIndexes.isEmpty()) {
        std::cout << "Line_Widget: No line selected to delete." << std::endl;
        return;
    }
    int selected_row = selectedIndexes.first().row();
    LineTableRow row_data = _line_table_model->getRowData(selected_row);

    if (row_data.frame == -1) {
        std::cout << "Line_Widget: Selected row data for deletion is invalid." << std::endl;
        return;
    }

    auto source_line_data = _data_manager->getData<LineData>(_active_key);
    if (!source_line_data) {
        std::cerr << "Line_Widget: Source LineData object ('" << _active_key << "') not found for deletion." << std::endl;
        return;
    }

    std::vector<Line2D> const & lines_at_frame = source_line_data->getLinesAtTime(row_data.frame);
    if (row_data.lineIndex < 0 || static_cast<size_t>(row_data.lineIndex) >= lines_at_frame.size()) {
        std::cerr << "Line_Widget: Line index out of bounds for deletion. Frame: " << row_data.frame 
                  << ", Index: " << row_data.lineIndex << std::endl;
        updateTable();
        return;
    }

    source_line_data->clearLineAtTime(row_data.frame, row_data.lineIndex);

    updateTable();
    _populateMoveToComboBox();

    std::cout << "Line deleted from " << _active_key << " frame " << row_data.frame << " index " << row_data.lineIndex << std::endl;
}
