#include "Point_Widget.hpp"
#include "ui_Point_Widget.h"

#include "DataManager.hpp"
#include "DataManager/Points/Point_Data.hpp"
#include "PointTableModel.hpp"
#include "IO_Widgets/Points/CSV/CSVPointSaver_Widget.hpp"
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
#include <QMenu>

#include <filesystem>
#include <fstream>
#include <iostream>
#include <set>
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
    ui->tableView->setContextMenuPolicy(Qt::CustomContextMenu);

    connect(ui->export_type_combo, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &Point_Widget::_onExportTypeChanged);

    connect(ui->csv_point_saver_widget, &CSVPointSaver_Widget::saveCSVRequested,
            this, &Point_Widget::_handleSaveCSVRequested);

    connect(ui->tableView, &QTableView::doubleClicked, this, &Point_Widget::_handleTableViewDoubleClicked);
    connect(ui->tableView, &QTableView::customContextMenuRequested, this, &Point_Widget::_showContextMenu);

    connect(ui->export_media_frames_checkbox, &QCheckBox::toggled, 
            this, &Point_Widget::_onExportMediaFramesCheckboxToggled);

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
}

void Point_Widget::updateTable() {
    // This function is called by _onDataChanged. It should refresh the table with current data.
    if (!_active_key.empty() && _point_table_model) {
        auto point_data = _data_manager->getData<PointData>(_active_key);
        _point_table_model->setPoints(point_data.get()); // Re-set points to refresh the model
    }
}

void Point_Widget::removeCallbacks() {
    remove_callback(_data_manager.get(), _active_key, _callback_id);
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

void Point_Widget::_showContextMenu(QPoint const& position) {
    QModelIndex index = ui->tableView->indexAt(position);
    if (!index.isValid()) {
        return;
    }

    QMenu context_menu(this);

    // Add move and copy submenus using the utility function
    auto move_callback = [this](std::string const& target_key) {
        _movePointsToTarget(target_key);
    };
    
    auto copy_callback = [this](std::string const& target_key) {
        _copyPointsToTarget(target_key);
    };

    add_move_copy_submenus<PointData>(&context_menu, _data_manager.get(), _active_key, move_callback, copy_callback);

    // Add separator and existing operations
    context_menu.addSeparator();
    QAction* delete_action = context_menu.addAction("Delete Selected Points");

    connect(delete_action, &QAction::triggered, this, &Point_Widget::_deleteSelectedPoints);

    context_menu.exec(ui->tableView->mapToGlobal(position));
}

std::vector<int> Point_Widget::_getSelectedFrames() {
    std::vector<int> selected_frames;
    QModelIndexList selected_rows = ui->tableView->selectionModel()->selectedRows();

    for (QModelIndex const& index : selected_rows) {
        if (index.isValid()) {
            int frame = _point_table_model->getFrameForRow(index.row());
            if (frame != -1) {
                selected_frames.push_back(frame);
            }
        }
    }

    return selected_frames;
}

void Point_Widget::_movePointsToTarget(std::string const& target_key) {
    std::vector<int> selected_frames = _getSelectedFrames();
    if (selected_frames.empty()) {
        std::cout << "Point_Widget: No points selected to move." << std::endl;
        return;
    }

    auto source_point_data = _data_manager->getData<PointData>(_active_key);
    auto target_point_data = _data_manager->getData<PointData>(target_key);

    if (!source_point_data) {
        std::cerr << "Point_Widget: Source PointData object ('" << _active_key << "') not found." << std::endl;
        return;
    }
    if (!target_point_data) {
        std::cerr << "Point_Widget: Target PointData object ('" << target_key << "') not found." << std::endl;
        return;
    }

    std::cout << "Point_Widget: Moving points from " << selected_frames.size() 
              << " frames from '" << _active_key << "' to '" << target_key << "'..." << std::endl;

    // Use the new moveTo method with the vector of selected times
    std::size_t total_points_moved = source_point_data->moveTo(*target_point_data, selected_frames, true);

    if (total_points_moved > 0) {
        // Update the table view to reflect changes
        updateTable();
        
        std::cout << "Point_Widget: Successfully moved " << total_points_moved 
                  << " points from " << selected_frames.size() << " frames." << std::endl;
    } else {
        std::cout << "Point_Widget: No points found in any of the selected frames to move." << std::endl;
    }
}

void Point_Widget::_copyPointsToTarget(std::string const& target_key) {
    std::vector<int> selected_frames = _getSelectedFrames();
    if (selected_frames.empty()) {
        std::cout << "Point_Widget: No points selected to copy." << std::endl;
        return;
    }

    auto source_point_data = _data_manager->getData<PointData>(_active_key);
    auto target_point_data = _data_manager->getData<PointData>(target_key);

    if (!source_point_data) {
        std::cerr << "Point_Widget: Source PointData object ('" << _active_key << "') not found." << std::endl;
        return;
    }
    if (!target_point_data) {
        std::cerr << "Point_Widget: Target PointData object ('" << target_key << "') not found." << std::endl;
        return;
    }

    std::cout << "Point_Widget: Copying points from " << selected_frames.size() 
              << " frames from '" << _active_key << "' to '" << target_key << "'..." << std::endl;

    // Use the new copyTo method with the vector of selected times
    std::size_t total_points_copied = source_point_data->copyTo(*target_point_data, selected_frames, true);

    if (total_points_copied > 0) {
        std::cout << "Point_Widget: Successfully copied " << total_points_copied 
                  << " points from " << selected_frames.size() << " frames." << std::endl;
    } else {
        std::cout << "Point_Widget: No points found in any of the selected frames to copy." << std::endl;
    }
}

void Point_Widget::_deleteSelectedPoints() {
    std::vector<int> selected_frames = _getSelectedFrames();
    if (selected_frames.empty()) {
        std::cout << "Point_Widget: No points selected to delete." << std::endl;
        return;
    }

    auto point_data_ptr = _data_manager->getData<PointData>(_active_key);
    if (!point_data_ptr) {
        std::cerr << "Point_Widget: Source PointData object ('" << _active_key << "') not found for deletion." << std::endl;
        return;
    }

    std::cout << "Point_Widget: Deleting points from " << selected_frames.size() 
              << " frames in '" << _active_key << "'..." << std::endl;

    int frames_with_points = 0;
    int total_points_deleted = 0;

    // Count points before deletion and batch operations to minimize observer notifications
    for (int frame : selected_frames) {
        auto points_at_frame = point_data_ptr->getPointsAtTime(frame);
        if (!points_at_frame.empty()) {
            frames_with_points++;
            total_points_deleted += static_cast<int>(points_at_frame.size());
            point_data_ptr->clearAtTime(frame, false);
        }
    }

    // Notify observers only once at the end
    if (frames_with_points > 0) {
        point_data_ptr->notifyObservers();
        
        // Update the table view to reflect changes
        updateTable();
        
        std::cout << "Point_Widget: Successfully deleted " << total_points_deleted 
                  << " points from " << frames_with_points << " frames." << std::endl;
        if (frames_with_points < selected_frames.size()) {
            std::cout << "Point_Widget: Note: " << (selected_frames.size() - frames_with_points) 
                      << " selected frames contained no points to delete." << std::endl;
        }
    } else {
        std::cout << "Point_Widget: No points found in any of the selected frames to delete." << std::endl;
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
        auto times_with_data = point_data_ptr->getTimesWithData();
        std::vector<size_t> frame_ids_to_export;
        frame_ids_to_export.reserve(times_with_data.size());
        for (int frame_id : times_with_data) {
            frame_ids_to_export.push_back(static_cast<size_t>(frame_id));
        }
        
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
