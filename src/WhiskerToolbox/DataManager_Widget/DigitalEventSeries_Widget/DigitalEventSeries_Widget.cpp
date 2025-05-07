#include "DigitalEventSeries_Widget.hpp"
#include "ui_DigitalEventSeries_Widget.h"

#include "EventTableModel.hpp"

#include "DataManager.hpp"
#include "DataManager/DigitalTimeSeries/Digital_Event_Series.hpp"

#include <QFileDialog>
#include <QKeyEvent>
#include <QMessageBox>
#include <QPushButton>

#include <filesystem>
#include <fstream>
#include <iostream>


DigitalEventSeries_Widget::DigitalEventSeries_Widget(std::shared_ptr<DataManager> data_manager, QWidget * parent)
    : QWidget(parent),
      ui(new Ui::DigitalEventSeries_Widget),
      _data_manager{std::move(data_manager)} {
    ui->setupUi(this);

    _event_table_model = new EventTableModel(this);
    ui->tableView->setModel(_event_table_model);

    ui->tableView->setEditTriggers(QAbstractItemView::SelectedClicked);

    //connect(ui->save_csv, &QPushButton::clicked, this, &DigitalEventSeries_Widget::_saveCSV);
    connect(ui->add_event_button, &QPushButton::clicked, this, &DigitalEventSeries_Widget::_addEventButton);
    connect(ui->remove_event_button, &QPushButton::clicked, this, &DigitalEventSeries_Widget::_removeEventButton);
    connect(ui->tableView, &QTableView::doubleClicked, this, &DigitalEventSeries_Widget::_handleCellClicked);
}

DigitalEventSeries_Widget::~DigitalEventSeries_Widget() {
    delete ui;
}

void DigitalEventSeries_Widget::openWidget() {
    // Populate the widget with data if needed
    this->show();
}

/*
void DigitalEventSeries_Widget::_saveCSV() {
    auto output_path = _data_manager->getOutputPath();
    if (output_path.empty()) {
        QString fileName = QFileDialog::getSaveFileName(this, tr("Save Events CSV"),
                                                        QString(), tr("CSV Files (*.csv)"));
        if (fileName.isEmpty())
            return;
        output_path = fileName.toStdString();
    } else {
        auto filename = ui->filename_textbox->text().toStdString();
        if (filename.empty()) {
            QMessageBox::warning(this, "Warning", "Please enter a filename");
            return;
        }
        output_path.append(filename);
    }

    auto events = _data_manager->getData<DigitalEventSeries>(_active_key)->getEventSeries();

    std::ofstream file(output_path);
    if (!file) {
        QMessageBox::critical(this, "Error", "Failed to open file for writing");
        return;
    }

    file << "Frame\n";
    for (const auto& event : events) {
        file << event << "\n";
    }

    QMessageBox::information(this, "Success", "Events saved successfully");
}
*/

void DigitalEventSeries_Widget::setActiveKey(std::string key) {
    _active_key = std::move(key);

    _data_manager->removeCallbackFromData(_active_key, _callback_id);

    _assignCallbacks();

    _calculateEvents();
}

void DigitalEventSeries_Widget::_changeDataTable(QModelIndex const & topLeft, QModelIndex const & bottomRight, QVector<int> const & roles) {
    auto events = _data_manager->getData<DigitalEventSeries>(_active_key);
    auto event_series = events->getEventSeries();

    for (int row = topLeft.row(); row <= bottomRight.row(); ++row) {
        float newTime = _event_table_model->getEvent(row);
        if (row < event_series.size()) {
            // Remove old event and add new one
            events->removeEvent(event_series[row]);
            events->addEvent(newTime);
        }
    }
}

void DigitalEventSeries_Widget::_assignCallbacks() {
    _callback_id = _data_manager->addCallbackToData(_active_key, [this]() {
        _calculateEvents();
    });
}

void DigitalEventSeries_Widget::removeCallbacks() {
    _data_manager->removeCallbackFromData(_active_key, _callback_id);
}

void DigitalEventSeries_Widget::_calculateEvents() {
    auto events = _data_manager->getData<DigitalEventSeries>(_active_key);
    if (!events) return;

    ui->total_events_label->setText(QString::number(events->size()));

    _event_table_model->setEvents(events->getEventSeries());
}


void DigitalEventSeries_Widget::_addEventButton() {
    auto frame_num = _data_manager->getTime()->getLastLoadedFrame();
    auto events = _data_manager->getData<DigitalEventSeries>(_active_key);

    if (!events) return;

    events->addEvent(static_cast<float>(frame_num));

    _calculateEvents();
}

void DigitalEventSeries_Widget::_removeEventButton() {
    auto frame_num = _data_manager->getTime()->getLastLoadedFrame();
    auto events = _data_manager->getData<DigitalEventSeries>(_active_key);

    if (!events) return;

    _calculateEvents();
}

void DigitalEventSeries_Widget::_handleCellClicked(QModelIndex const & index) {
    if (!index.isValid()) {
        return;
    }

    // Get the frame number from the clicked cell
    float frameNumber = _event_table_model->getEvent(index.row());

    // Emit the signal with the frame number
    emit frameSelected(static_cast<int>(frameNumber));
}
