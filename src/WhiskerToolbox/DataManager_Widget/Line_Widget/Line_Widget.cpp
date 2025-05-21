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
