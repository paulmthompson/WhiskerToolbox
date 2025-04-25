#include "DigitalIntervalSeries_Widget.hpp"
#include "ui_DigitalIntervalSeries_Widget.h"

#include "DataManager.hpp"
#include "DataManager/DigitalTimeSeries/Digital_Interval_Series.hpp"
#include "IntervalTableModel.hpp"

#include <QEvent>
#include <QItemDelegate>
#include <QKeyEvent>
#include <QPushButton>

#include <filesystem>
#include <iostream>


DigitalIntervalSeries_Widget::DigitalIntervalSeries_Widget(std::shared_ptr<DataManager> data_manager, QWidget * parent)
    : QWidget(parent),
      ui(new Ui::DigitalIntervalSeries_Widget),
      _data_manager{std::move(data_manager)} {
    ui->setupUi(this);

    _interval_table_model = new IntervalTableModel(this);
    ui->tableView->setModel(_interval_table_model);

    ui->tableView->setEditTriggers(QAbstractItemView::SelectedClicked);

    connect(ui->save_csv, &QPushButton::clicked, this, &DigitalIntervalSeries_Widget::_saveCSV);
    connect(ui->create_interval_button, &QPushButton::clicked, this, &DigitalIntervalSeries_Widget::_createIntervalButton);
    connect(ui->remove_interval_button, &QPushButton::clicked, this, &DigitalIntervalSeries_Widget::_removeIntervalButton);
    connect(ui->flip_single_frame, &QPushButton::clicked, this, &DigitalIntervalSeries_Widget::_flipIntervalButton);
    connect(ui->tableView, &QTableView::doubleClicked, this, &DigitalIntervalSeries_Widget::_handleCellClicked);
    connect(ui->extend_interval_button, &QPushButton::clicked, this, &DigitalIntervalSeries_Widget::_extendInterval);
}

DigitalIntervalSeries_Widget::~DigitalIntervalSeries_Widget() {
    delete ui;
}

void DigitalIntervalSeries_Widget::openWidget() {
    // Populate the widget with data if needed
    this->show();
}

void DigitalIntervalSeries_Widget::_saveCSV() {
    auto output_path = _data_manager->getOutputPath();
    std::cout << output_path.string() << std::endl;
    std::cout << _active_key << std::endl;

    auto filename = ui->filename_textbox->text().toStdString();
    output_path.append(filename);

    auto contactEvents = _data_manager->getData<DigitalIntervalSeries>(_active_key)->getDigitalIntervalSeries();

    save_intervals(contactEvents, output_path.string());
}

void DigitalIntervalSeries_Widget::setActiveKey(std::string key) {
    _active_key = std::move(key);

    _data_manager->removeCallbackFromData(_active_key, _callback_id);

    _assignCallbacks();

    _calculateIntervals();
}

void DigitalIntervalSeries_Widget::_changeDataTable(QModelIndex const & topLeft, QModelIndex const & bottomRight, QVector<int> const & roles) {

    auto intervals = _data_manager->getData<DigitalIntervalSeries>(_active_key);

    for (int row = topLeft.row(); row <= bottomRight.row(); ++row) {
        Interval const interval = _interval_table_model->getInterval(row);
        std::cout << "Interval changed to " << interval.start << " , " << interval.end << std::endl;
        intervals->addEvent(interval.start, interval.end);
    }
}

void DigitalIntervalSeries_Widget::_assignCallbacks() {
    _callback_id = _data_manager->addCallbackToData(_active_key, [this]() {
        _calculateIntervals();
    });

    //auto intervals = _data_manager->getData<DigitalIntervalSeries>(_active_key);

    //connect(_interval_table_model, &IntervalTableModel::dataChanged, this, &DigitalIntervalSeries_Widget::_changeDataTable);
}

void DigitalIntervalSeries_Widget::removeCallbacks() {
    //disconnect(_interval_table_model, &IntervalTableModel::dataChanged, this, &DigitalIntervalSeries_Widget::_changeDataTable);
}

void DigitalIntervalSeries_Widget::_calculateIntervals() {
    auto intervals = _data_manager->getData<DigitalIntervalSeries>(_active_key);
    ui->total_interval_label->setText(QString::number(intervals->size()));

    _interval_table_model->setIntervals(intervals->getDigitalIntervalSeries());
}


void DigitalIntervalSeries_Widget::_createIntervalButton() {
    auto frame_num = _data_manager->getTime()->getLastLoadedFrame();


    //Convert to clock coordinates
    //auto time_key = _data_manager->getTimeFrame(_active_key);
    //auto time_frame = _data_manager->getTime(time_key);

    //frame_num = time_frame->getTimeAtIndex(frame_num);

    auto contactIntervals = _data_manager->getData<DigitalIntervalSeries>(_active_key);

    if (_interval_epoch) {

        _interval_epoch = false;

        ui->create_interval_button->setText("Create Interval");

        contactIntervals->addEvent(_interval_start, frame_num);

    } else {
        _interval_start = frame_num;

        _interval_epoch = true;

        ui->create_interval_button->setText("Mark Interval End");
    }
}

void DigitalIntervalSeries_Widget::_removeIntervalButton() {
    auto frame_num = _data_manager->getTime()->getLastLoadedFrame();
    auto intervals = _data_manager->getData<DigitalIntervalSeries>(_active_key);

    // If we are in a contact epoch, we need to mark the termination frame and add those to block
    if (_interval_epoch) {

        _interval_epoch = false;

        ui->remove_interval_button->setText("Remove Interval");

        for (int i = _interval_start; i < frame_num; i++) {
            intervals->setEventAtTime(i, false);
        }

    } else {
        _interval_start = frame_num;

        _interval_epoch = true;

        ui->remove_interval_button->setText("Mark Remove Interval End");
    }
}

void DigitalIntervalSeries_Widget::_flipIntervalButton() {

    auto frame_num = _data_manager->getTime()->getLastLoadedFrame();
    auto intervals = _data_manager->getData<DigitalIntervalSeries>(_active_key);

    if (intervals->isEventAtTime(frame_num)) {
        intervals->setEventAtTime(frame_num, false);
    } else {
        intervals->setEventAtTime(frame_num, true);
    }
}

void DigitalIntervalSeries_Widget::_handleCellClicked(QModelIndex const & index) {
    if (!index.isValid()) {
        return;
    }

    // Assuming the frame number is stored in the clicked cell
    int const frameNumber = index.data().toInt();

    // Emit the signal with the frame number
    emit frameSelected(frameNumber);
}

void DigitalIntervalSeries_Widget::_extendInterval() {
    auto selectedIndexes = ui->tableView->selectionModel()->selectedIndexes();

    if (selectedIndexes.isEmpty()) {
        std::cout << "Error: No frame selected in the table view." << std::endl;
        return;
    }

    auto currentFrame = _data_manager->getTime()->getLastLoadedFrame();
    auto intervals = _data_manager->getData<DigitalIntervalSeries>(_active_key);

    for (auto const & index: selectedIndexes) {
        if (index.column() == 0) {// Only process the first column to avoid duplicate processing
            Interval const interval = _interval_table_model->getInterval(index.row());

            if (currentFrame < interval.start) {
                intervals->addEvent(Interval{currentFrame, interval.end});
            } else if (currentFrame > interval.end) {
                intervals->addEvent(Interval{interval.start, currentFrame});
            } else {
                std::cout << "Error: Current frame is within the selected interval." << std::endl;
            }
        }
    }
}
