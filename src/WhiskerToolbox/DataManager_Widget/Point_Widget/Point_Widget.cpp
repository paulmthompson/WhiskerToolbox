#include "Point_Widget.hpp"
#include "ui_Point_Widget.h"

#include "DataManager.hpp"
#include "DataManager/Points/Point_Data.hpp"
#include "PointTableModel.hpp"
#include "IO_Widgets/PointLoaderWidget/CSV/CSVPointSaver_Widget.hpp"
#include "IO_Widgets/Media/MediaExport_Widget.hpp"
#include "DataManager_Widget/utils/DataManager_Widget_utils.hpp"

#include <QFileDialog>
#include <QPushButton>
#include <QTableView>
#include <QComboBox>
#include <QStackedWidget>
#include <QMessageBox>
#include <QCheckBox>
#include <QDir>

#include <filesystem>
#include <fstream>
#include <iostream>
#include <variant>

Point_Widget::Point_Widget(std::shared_ptr<DataManager> data_manager, QWidget * parent)
    : QWidget(parent),
      ui(new Ui::Point_Widget),
      _data_manager{std::move(data_manager)},
      _point_table_model{new PointTableModel{this}} {
    ui->setupUi(this);

    ui->tableView->setModel(_point_table_model);
    ui->tableView->setSelectionBehavior(QAbstractItemView::SelectRows);
    ui->tableView->setEditTriggers(QAbstractItemView::NoEditTriggers);

    connect(ui->export_type_combo, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &Point_Widget::_onExportTypeChanged);

    connect(ui->csv_point_saver_widget, &CSVPointSaver_Widget::saveCSVRequested,
            this, &Point_Widget::_handleSaveCSVRequested);

    connect(ui->tableView, &QTableView::doubleClicked, this, &Point_Widget::_handleTableViewDoubleClicked);
    connect(ui->movePointsButton, &QPushButton::clicked, this, &Point_Widget::_movePointsButton_clicked);
    connect(ui->deletePointsButton, &QPushButton::clicked, this, &Point_Widget::_deletePointsButton_clicked);

    connect(ui->export_media_frames_checkbox, &QCheckBox::toggled, 
            this, &Point_Widget::_onExportMediaFramesCheckboxToggled);

    _populateMoveToPointDataComboBox();

    _onExportTypeChanged(ui->export_type_combo->currentIndex());
    ui->media_export_options_widget->setVisible(ui->export_media_frames_checkbox->isChecked());
}

Point_Widget::~Point_Widget() {
    removeCallbacks();
    delete ui;
}

void Point_Widget::openWidget() {
    this->show();
    this->activateWindow();
}

void Point_Widget::setActiveKey(std::string const & key) {
    if (_active_key == key && _data_manager->getData<PointData>(key)) {
        // If the key is the same and data exists, a full reset might not be needed,
        // but for simplicity and to ensure correct state, we proceed.
        // updateTable(); // This would be a lighter refresh if data object hasn't changed
        // _populateMoveToPointDataComboBox();
        // return;
    }
    removeCallbacks();
    _active_key = key;
    auto point_data = _data_manager->getData<PointData>(_active_key);
    if (point_data) {
        _point_table_model->setPoints(point_data.get()); // Correctly set points here
        _callback_id = point_data->addObserver([this]() { _onDataChanged(); });
    } else {
        std::cerr << "Point_Widget: Could not set active key '" << _active_key << "' as PointData is null." << std::endl;
        _point_table_model->setPoints(nullptr); // Clear table if data is not available
    }
    // updateTable(); // No longer needed here as setPoints above resets model
    _populateMoveToPointDataComboBox();
}

void Point_Widget::updateTable() {
    // This function is called by _onDataChanged. It should refresh the table with current data.
    if (!_active_key.empty() && _point_table_model) {
        auto point_data = _data_manager->getData<PointData>(_active_key);
        _point_table_model->setPoints(point_data.get()); // Re-set points to refresh the model
    }
}

void Point_Widget::loadFrame(int frame_id) {
    if (_previous_frame != frame_id) {
        if (ui->propagate_checkbox->isChecked()) {
            _propagateLabel(frame_id);
        }
        // Removed: _point_table_model->setActiveFrame(frame_id); as it does not exist
        // The table model displays all frames; Point_Widget tracks the active frame via _previous_frame.
        _previous_frame = frame_id;
        // If the table needs to visually indicate the active frame, that would be a separate feature.
    }
}

void Point_Widget::removeCallbacks() {
    remove_callback(_data_manager.get(), _active_key, _callback_id);
}

void Point_Widget::_propagateLabel(int frame_id) {
    if (!_active_key.empty()) {
        auto point_data_ptr = _data_manager->getData<PointData>(_active_key);
        if (point_data_ptr) {
            auto const & points_at_previous_frame = point_data_ptr->getPointsAtTime(_previous_frame);
            if (!points_at_previous_frame.empty()){
                 point_data_ptr->overwritePointsAtTime(frame_id, points_at_previous_frame);
            }
        }
    }
}

void Point_Widget::_populateMoveToPointDataComboBox() {
    populate_move_combo_box<PointData>(ui->moveToPointDataComboBox, _data_manager.get(), _active_key);
}

void Point_Widget::_handleTableViewDoubleClicked(QModelIndex const & index) {
    if (index.isValid()) {
        int frame = _point_table_model->getFrameForRow(index.row());
        if (frame != -1) {
             emit frameSelected(frame);
        } else {
            emit frameSelected(index.row()); 
        }
    }
}

void Point_Widget::_movePointsButton_clicked() {
    if (!_active_key.empty() && ui->moveToPointDataComboBox->count() > 0) {
        auto current_pd = _data_manager->getData<PointData>(_active_key);
        std::string target_key = ui->moveToPointDataComboBox->currentText().toStdString();
        auto target_pd = _data_manager->getData<PointData>(target_key);

        if (current_pd && target_pd) {
            // int current_frame = _point_table_model->getActiveFrame(); // Replaced
            int current_frame = _previous_frame; // Use Point_Widget's tracked active frame
            auto points_to_move = current_pd->getPointsAtTime(current_frame);
            if (!points_to_move.empty()) {
                target_pd->addPointsAtTime(current_frame, points_to_move);
                current_pd->clearAtTime(current_frame);
            }
        }
    }
}

void Point_Widget::_deletePointsButton_clicked() {
    if (!_active_key.empty()) {
        auto point_data_ptr = _data_manager->getData<PointData>(_active_key);
        if (point_data_ptr) {
            // point_data_ptr->clearPointsAtTime(_point_table_model->getActiveFrame()); // Replaced
            point_data_ptr->clearAtTime(_previous_frame); // Use Point_Widget's tracked active frame
        }
    }
}

void Point_Widget::_onDataChanged() {
    updateTable();
}

void Point_Widget::_onExportTypeChanged(int index) {
    QString current_text = ui->export_type_combo->itemText(index);
    if (current_text == "CSV") {
        ui->stacked_saver_options->setCurrentWidget(ui->csv_point_saver_widget);
    }
}

void Point_Widget::_handleSaveCSVRequested(CSVPointSaverOptions csv_options) {
    PointSaverOptionsVariant options_variant = csv_options;
    _initiateSaveProcess(SaverType::CSV, options_variant);
}

void Point_Widget::_initiateSaveProcess(SaverType saver_type, PointSaverOptionsVariant& options_variant) {
    if (_active_key.empty() || !_data_manager->getData<PointData>(_active_key)) {
        QMessageBox::warning(this, "No Data", "No active point data to save.");
        return;
    }

    auto point_data_ptr = _data_manager->getData<PointData>(_active_key);
    if (!point_data_ptr) {
        QMessageBox::critical(this, "Error", "Could not retrieve data for saving.");
        return;
    }

    bool save_successful = false;
    switch (saver_type) {
        case SaverType::CSV: {
            CSVPointSaverOptions& specific_csv_options = std::get<CSVPointSaverOptions>(options_variant);
            specific_csv_options.parent_dir = _data_manager->getOutputPath().string();
            save_successful = _performActualCSVSave(specific_csv_options);
            break;
        }
    }

    if (!save_successful) {
        QMessageBox::critical(this, "Save Error", "Failed to save point data.");
        return; 
    }

    if (ui->export_media_frames_checkbox->isChecked()) {
        std::vector<size_t> frame_ids_to_export = point_data_ptr->getTimesWithData();
        export_media_frames(_data_manager.get(),
                            ui->media_export_options_widget,
                            options_variant,
                            this,
                            frame_ids_to_export);

    }
}

bool Point_Widget::_performActualCSVSave(CSVPointSaverOptions & options) {
    auto point_data_ptr = _data_manager->getData<PointData>(_active_key);
    if (!point_data_ptr) {
        std::cerr << "_performActualCSVSave: Critical - Could not get PointData for key: " << _active_key << std::endl;
        return false;
    }

    save(point_data_ptr.get(), options); 
    QMessageBox::information(this, "Save Successful", QString::fromStdString("Points data saved to " + options.parent_dir + "/" + options.filename));
    std::cout << "Point data saved to: " << options.parent_dir << "/" << options.filename << std::endl;
    return true; 
}

void Point_Widget::_onExportMediaFramesCheckboxToggled(bool checked) {
    ui->media_export_options_widget->setVisible(checked);
}
