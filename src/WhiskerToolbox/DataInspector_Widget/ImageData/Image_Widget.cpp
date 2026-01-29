#include "Image_Widget.hpp"
#include "ui_Image_Widget.h"

#include "DataManager/DataManager.hpp"
#include "DataManager/Media/Image_Data.hpp"
#include "DataManager/Media/Media_Data.hpp"
#include "DataManager_Widget/utils/DataManager_Widget_utils.hpp"
#include "ImageTableModel.hpp"

#include <QModelIndex>
#include <QTableView>

#include <iostream>

Image_Widget::Image_Widget(std::shared_ptr<DataManager> data_manager, QWidget * parent)
    : QWidget(parent),
      ui(new Ui::Image_Widget),
      _data_manager{std::move(data_manager)} {
    ui->setupUi(this);

    _image_table_model = new ImageTableModel(this);
    ui->tableView->setModel(_image_table_model);
    ui->tableView->setSelectionBehavior(QAbstractItemView::SelectRows);
    ui->tableView->setSelectionMode(QAbstractItemView::SingleSelection);
    ui->tableView->setEditTriggers(QAbstractItemView::NoEditTriggers);

    connect(ui->tableView, &QTableView::doubleClicked, this, &Image_Widget::_handleTableViewDoubleClicked);
}

Image_Widget::~Image_Widget() {
    removeCallbacks();
    delete ui;
}

void Image_Widget::openWidget() {
    this->show();
}

void Image_Widget::setActiveKey(std::string const & key) {
    if (_active_key == key && _callback_id != -1) {
        updateTable();
        return;
    }
    removeCallbacks();

    _active_key = key;
    updateTable();

    if (!_active_key.empty()) {
        auto media_data = _data_manager->getData<MediaData>(_active_key);
        if (media_data) {
            _callback_id = _data_manager->addCallbackToData(_active_key, [this]() { _onDataChanged(); });
        } else {
            std::cerr << "Image_Widget: No ImageData found for key '" << _active_key << "' to attach callback." << std::endl;
        }
    }
}

void Image_Widget::updateTable() {
    if (!_active_key.empty()) {
        auto media_data = _data_manager->getData<MediaData>(_active_key);
        auto image_data = dynamic_cast<ImageData *>(media_data.get());
        _image_table_model->setImages(image_data);
    } else {
        _image_table_model->setImages(nullptr);
    }
}

void Image_Widget::removeCallbacks() {
    remove_callback(_data_manager.get(), _active_key, _callback_id);
}

void Image_Widget::_onDataChanged() {
    updateTable();
}

void Image_Widget::_handleTableViewDoubleClicked(QModelIndex const & index) {
    if (!index.isValid()) return;
    auto tf = _data_manager->getTime(TimeKey(_active_key));
    int frame = _image_table_model->getFrameForRow(index.row());
    if (frame != -1) {
        emit frameSelected(TimePosition(TimeFrameIndex(frame), tf));
    }
}