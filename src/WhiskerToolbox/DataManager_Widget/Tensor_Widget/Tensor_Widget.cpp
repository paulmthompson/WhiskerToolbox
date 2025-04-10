#include "Tensor_Widget.hpp"
#include "ui_Tensor_Widget.h"

#include "DataManager.hpp"

//https://stackoverflow.com/questions/72533139/libtorch-errors-when-used-with-qt-opencv-and-point-cloud-library
#undef slots
#include "DataManager/Tensors/Tensor_Data.hpp"
#define slots Q_SLOTS

#include "TensorTableModel.hpp"

#include <QFileDialog>
#include <QPlainTextEdit>
#include <QPushButton>

#include <fstream>
#include <iostream>

Tensor_Widget::Tensor_Widget(std::shared_ptr<DataManager> data_manager, QWidget * parent)
    : QWidget(parent),
      ui(new Ui::Tensor_Widget),
      _data_manager{std::move(data_manager)} {
    ui->setupUi(this);

    _tensor_table_model = new TensorTableModel(this);
    ui->tableView->setModel(_tensor_table_model);

    //connect(ui->save_csv_button, &QPushButton::clicked, this, &Tensor_Widget::_saveTensorCSV);
}

Tensor_Widget::~Tensor_Widget() {
    delete ui;
}

void Tensor_Widget::openWidget() {
    // Populate the widget with data if needed
    this->show();
}

void Tensor_Widget::setActiveKey(std::string const & key) {
    _active_key = key;
    updateTable();
}

void Tensor_Widget::updateTable() {
    auto tensors = _data_manager->getData<TensorData>(_active_key)->getData();
    _tensor_table_model->setTensors(tensors);
}

void Tensor_Widget::_saveTensorCSV() {

    /*
    const auto filename = ui->save_filename->toPlainText().toStdString();

    std::fstream fout;

    auto frame_by_frame_output = _data_manager->getOutputPath();

    fout.open(frame_by_frame_output.append(filename).string(), std::fstream::out);

    auto tensor_data = _data_manager->getData<TensorData>(_active_key)->getData();

    for (const auto& [key, val] : tensor_data)
    {
        fout << key << "," << val.sizes() << "\n";
    }

    fout.close();
    */
}
