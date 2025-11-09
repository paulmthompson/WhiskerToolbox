#include "Point_Widget.hpp"
#include "ui_Point_Widget.h"

#include "DataManager.hpp"
#include "DataManager/Points/Point_Data.hpp"
#include "DataManager/Media/Media_Data.hpp"
#include "DataManager_Widget/utils/DataManager_Widget_utils.hpp"
#include "MediaExport/MediaExport_Widget.hpp"
#include "MediaExport/media_export.hpp"
#include "IO_Widgets/Points/CSV/CSVPointSaver_Widget.hpp"
#include "PointTableModel.hpp"
#include "WhiskerToolbox/GroupManagementWidget/GroupManager.hpp"

#include "CoreGeometry/ImageSize.hpp"

#include <QCheckBox>
#include <QComboBox>
#include <QDir>
#include <QFileDialog>
#include <QLineEdit>
#include <QMenu>
#include <QMessageBox>
#include <QPushButton>
#include <QStackedWidget>
#include <QTableView>

#include <filesystem>
#include <fstream>
#include <iostream>
#include <unordered_set>
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
    connect(ui->apply_image_size_button, &QPushButton::clicked,
            this, &Point_Widget::_onApplyImageSizeClicked);
    connect(ui->copy_image_size_button, &QPushButton::clicked,
            this, &Point_Widget::_onCopyImageSizeClicked);
    connect(ui->groupFilterCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &Point_Widget::_onGroupFilterChanged);

    // Setup collapsible export section
    ui->export_section->autoSetContentLayout();
    ui->export_section->setTitle("Export Options");
    ui->export_section->toggle(false); // Start collapsed

    _onExportTypeChanged(ui->export_type_combo->currentIndex());
    ui->media_export_options_widget->setVisible(ui->export_media_frames_checkbox->isChecked());
    
    // Populate media combo box
    _populateMediaComboBox();
    
    // Set up callback to refresh media combo box when data changes
    if (_data_manager) {
        _data_manager->addObserver([this]() {
            _populateMediaComboBox();
        });
    }
}

Point_Widget::~Point_Widget() {
    removeCallbacks();
    delete ui;
}

void Point_Widget::openWidget() {
    this->show();
    this->activateWindow();
    // Update image size display when widget is opened
    _updateImageSizeDisplay();
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
        _point_table_model->setPoints(point_data.get());// Correctly set points here
        _callback_id = point_data->addObserver([this]() { _onDataChanged(); });
        _updateImageSizeDisplay();
    } else {
        std::cerr << "Point_Widget: Could not set active key '" << _active_key << "' as PointData is null." << std::endl;
        _point_table_model->setPoints(nullptr);// Clear table if data is not available
    }
    // updateTable(); // No longer needed here as setPoints above resets model
}

void Point_Widget::updateTable() {
    // This function is called by _onDataChanged. It should refresh the table with current data.
    if (!_active_key.empty() && _point_table_model) {
        auto point_data = _data_manager->getData<PointData>(_active_key);
        _point_table_model->setPoints(point_data.get());// Re-set points to refresh the model
    }
}

void Point_Widget::removeCallbacks() {
    remove_callback(_data_manager.get(), _active_key, _callback_id);
}

void Point_Widget::_handleTableViewDoubleClicked(QModelIndex const & index) {
    if (index.isValid()) {
        PointTableRow const row_data = _point_table_model->getRowData(index.row());
        if (row_data.frame != -1) {
            emit frameSelected(static_cast<int>(row_data.frame));
        }
    }
}

void Point_Widget::_showContextMenu(QPoint const & position) {
    QModelIndex const index = ui->tableView->indexAt(position);
    if (!index.isValid()) {
        return;
    }

    QMenu context_menu(this);

    // Add move and copy submenus using the utility function
    auto move_callback = [this](std::string const & target_key) {
        _movePointsToTarget(target_key);
    };

    auto copy_callback = [this](std::string const & target_key) {
        _copyPointsToTarget(target_key);
    };

    add_move_copy_submenus<PointData>(&context_menu, _data_manager.get(), _active_key, move_callback, copy_callback);

    // Add group management options
    context_menu.addSeparator();
    QMenu * group_menu = context_menu.addMenu("Group Management");
    
    // Add "Move to Group" submenu
    QMenu * move_to_group_menu = group_menu->addMenu("Move to Group");
    _populateGroupSubmenu(move_to_group_menu, true);
    
    // Add "Remove from Group" action
    QAction * remove_from_group_action = group_menu->addAction("Remove from Group");
    connect(remove_from_group_action, &QAction::triggered, this, &Point_Widget::_removeSelectedPointsFromGroup);

    // Add separator and existing operations
    context_menu.addSeparator();
    QAction * delete_action = context_menu.addAction("Delete Selected Points");

    connect(delete_action, &QAction::triggered, this, &Point_Widget::_deleteSelectedPoints);

    context_menu.exec(ui->tableView->mapToGlobal(position));
}

std::vector<TimeFrameIndex> Point_Widget::_getSelectedFrames() {
    std::vector<TimeFrameIndex> selected_frames;
    QModelIndexList const selected_rows = ui->tableView->selectionModel()->selectedRows();

    for (QModelIndex const & index: selected_rows) {
        if (index.isValid()) {
            PointTableRow const row_data = _point_table_model->getRowData(index.row());
            if (row_data.frame != -1) {
                selected_frames.emplace_back(static_cast<int>(row_data.frame));
            }
        }
    }

    return selected_frames;
}

std::vector<EntityId> Point_Widget::_getSelectedEntityIds() {
    QModelIndexList const selectedIndexes = ui->tableView->selectionModel()->selectedRows();
    std::vector<EntityId> entity_ids;

    for (auto const & index: selectedIndexes) {
        if (index.isValid()) {
            PointTableRow const row_data = _point_table_model->getRowData(index.row());
            if (row_data.entity_id != 0) { // Valid entity ID
                entity_ids.push_back(row_data.entity_id);
            }
        }
    }

    return entity_ids;
}

void Point_Widget::_movePointsToTarget(std::string const & target_key) {
    auto selected_entity_ids = _getSelectedEntityIds();
    if (selected_entity_ids.empty()) {
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

    std::cout << "Point_Widget: Moving " << selected_entity_ids.size()
              << " selected points from '" << _active_key << "' to '" << target_key << "'..." << std::endl;

    // Use the moveByEntityIds method to move only the selected points
    std::unordered_set<EntityId> const selected_entity_ids_set(selected_entity_ids.begin(), selected_entity_ids.end());
    std::size_t const total_points_moved = source_point_data->moveByEntityIds(*target_point_data, selected_entity_ids_set, true);

    if (total_points_moved > 0) {
        // Update the table view to reflect changes
        updateTable();

        std::cout << "Point_Widget: Successfully moved " << total_points_moved
                  << " selected points." << std::endl;
    } else {
        std::cout << "Point_Widget: No points found with the selected EntityIds to move." << std::endl;
    }
}

void Point_Widget::_copyPointsToTarget(std::string const & target_key) {
    auto selected_entity_ids = _getSelectedEntityIds();
    if (selected_entity_ids.empty()) {
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

    std::cout << "Point_Widget: Copying " << selected_entity_ids.size()
              << " selected points from '" << _active_key << "' to '" << target_key << "'..." << std::endl;

    // Use the copyPointsByEntityIds method to copy only the selected points
    std::unordered_set<EntityId> const selected_entity_ids_set2(selected_entity_ids.begin(), selected_entity_ids.end());
    std::size_t const total_points_copied = source_point_data->copyByEntityIds(*target_point_data, selected_entity_ids_set2, true);

    if (total_points_copied > 0) {
        std::cout << "Point_Widget: Successfully copied " << total_points_copied
                  << " selected points." << std::endl;
    } else {
        std::cout << "Point_Widget: No points found with the selected EntityIds to copy." << std::endl;
    }
}

void Point_Widget::_deleteSelectedPoints() {
    QModelIndexList selectedIndexes = ui->tableView->selectionModel()->selectedRows();
    if (selectedIndexes.isEmpty()) {
        std::cout << "Point_Widget: No points selected to delete." << std::endl;
        return;
    }

    auto point_data_ptr = _data_manager->getData<PointData>(_active_key);
    if (!point_data_ptr) {
        std::cerr << "Point_Widget: Source PointData object ('" << _active_key << "') not found for deletion." << std::endl;
        return;
    }

    std::cout << "Point_Widget: Deleting " << selectedIndexes.size()
              << " selected points from '" << _active_key << "'..." << std::endl;

    int total_points_deleted = 0;

    // Delete each selected point individually
    for (auto const & index: selectedIndexes) {
        if (index.isValid()) {
            PointTableRow const row_data = _point_table_model->getRowData(index.row());
            if (row_data.frame != -1 && row_data.pointIndex >= 0) {
                bool const success = point_data_ptr->clearByEntityId(row_data.entity_id, false);
                if (success) {
                    total_points_deleted++;
                }
            }
        }
    }

    // Notify observers only once at the end
    if (total_points_deleted > 0) {
        point_data_ptr->notifyObservers();

        // Update the table view to reflect changes
        updateTable();

        std::cout << "Point_Widget: Successfully deleted " << total_points_deleted
                  << " selected points." << std::endl;
    } else {
        std::cout << "Point_Widget: No points found to delete from the selected rows." << std::endl;
    }
}

void Point_Widget::_onDataChanged() {
    updateTable();
}

void Point_Widget::_onExportTypeChanged(int index) {
    QString const current_text = ui->export_type_combo->itemText(index);
    if (current_text == "CSV") {
        ui->stacked_saver_options->setCurrentWidget(ui->csv_point_saver_widget);
    }
}

void Point_Widget::_handleSaveCSVRequested(CSVPointSaverOptions csv_options) {
    PointSaverOptionsVariant options_variant = csv_options;
    _initiateSaveProcess(SaverType::CSV, options_variant);
}

void Point_Widget::_initiateSaveProcess(SaverType saver_type, PointSaverOptionsVariant & options_variant) {
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
            auto & specific_csv_options = std::get<CSVPointSaverOptions>(options_variant);
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
        for (auto frame_id: times_with_data) {
            frame_ids_to_export.push_back(static_cast<size_t>(frame_id.getValue()));
        }

        if (!frame_ids_to_export.empty()) {
            auto media_ptr = _data_manager->getData<MediaData>("media");
            if (!media_ptr) {
                QMessageBox::warning(this, "Media Not Available", "Could not access media for exporting frames.");
            } else {
                // Derive output dir from point save options (CSV in this branch)
                std::string base_output_dir;
                if (std::holds_alternative<CSVPointSaverOptions>(options_variant)) {
                    base_output_dir = std::get<CSVPointSaverOptions>(options_variant).parent_dir;
                } else {
                    base_output_dir = _data_manager->getOutputPath().string();
                }

                MediaExportOptions options = ui->media_export_options_widget->getOptions();
                options.image_save_dir = base_output_dir;

                try {
                    std::filesystem::create_directories(options.image_save_dir);
                } catch (std::exception const & e) {
                    QMessageBox::critical(this, "Export Error", QString("Failed to create output directory: %1\n%2")
                                                                   .arg(QString::fromStdString(options.image_save_dir))
                                                                   .arg(QString::fromStdString(e.what())));
                    return;
                }

                int frames_exported = 0;
                for (size_t const frame_id: frame_ids_to_export) {
                    save_image(media_ptr.get(), static_cast<int>(frame_id), options);
                    frames_exported++;
                }

                QMessageBox::information(this,
                                         "Media Export",
                                         QString("Exported %1 media frames to: %2/%3")
                                             .arg(frames_exported)
                                             .arg(QString::fromStdString(options.image_save_dir))
                                             .arg(QString::fromStdString(options.image_folder)));
            }
        } else {
            QMessageBox::information(this, "No Frames", "No points found in data, so no media frames to export.");
        }
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

void Point_Widget::_onApplyImageSizeClicked() {
    if (_active_key.empty()) {
        QMessageBox::warning(this, "No Data Selected", "Please select a PointData item to modify image size.");
        return;
    }

    auto point_data = _data_manager->getData<PointData>(_active_key);
    if (!point_data) {
        QMessageBox::critical(this, "Error", "Could not retrieve PointData for image size modification. Key: " + QString::fromStdString(_active_key));
        return;
    }

    // Get current values from the line edits
    QString width_text = ui->image_width_edit->text().trimmed();
    QString height_text = ui->image_height_edit->text().trimmed();

    if (width_text.isEmpty() || height_text.isEmpty()) {
        QMessageBox::warning(this, "Invalid Input", "Please enter both width and height values.");
        return;
    }

    bool width_ok, height_ok;
    int new_width = width_text.toInt(&width_ok);
    int new_height = height_text.toInt(&height_ok);

    if (!width_ok || !height_ok) {
        QMessageBox::warning(this, "Invalid Input", "Please enter valid integer values for width and height.");
        return;
    }

    if (new_width <= 0 || new_height <= 0) {
        QMessageBox::warning(this, "Invalid Input", "Width and height must be positive values.");
        return;
    }

    // Get current image size
    ImageSize current_size = point_data->getImageSize();
    
    // If no current size is set, just set the new size without scaling
    if (current_size.width == -1 || current_size.height == -1) {
        point_data->setImageSize({new_width, new_height});
        _updateImageSizeDisplay();
        QMessageBox::information(this, "Image Size Set", 
            QString("Image size set to %1 × %2 (no scaling applied as no previous size was set).")
                .arg(new_width).arg(new_height));
        return;
    }

    // Ask user if they want to scale existing data
    int ret = QMessageBox::question(this, "Scale Existing Data", 
        QString("Current image size is %1 × %2. Do you want to scale all existing point data to the new size %3 × %4?\n\n"
               "Click 'Yes' to scale all point data proportionally.\n"
               "Click 'No' to just change the image size without scaling.\n"
               "Click 'Cancel' to abort the operation.")
            .arg(current_size.width).arg(current_size.height)
            .arg(new_width).arg(new_height),
        QMessageBox::Yes | QMessageBox::No | QMessageBox::Cancel);

    if (ret == QMessageBox::Cancel) {
        return;
    }

    if (ret == QMessageBox::Yes) {
        // Scale the data
        point_data->changeImageSize({new_width, new_height});
        QMessageBox::information(this, "Image Size Changed", 
            QString("Image size changed to %1 × %2 and all point data has been scaled proportionally.")
                .arg(new_width).arg(new_height));
    } else {
        // Just set the new size without scaling
        point_data->setImageSize({new_width, new_height});
        QMessageBox::information(this, "Image Size Set", 
            QString("Image size set to %1 × %2 (existing point data was not scaled).")
                .arg(new_width).arg(new_height));
    }

    _updateImageSizeDisplay();
}

void Point_Widget::_updateImageSizeDisplay() {
    if (_active_key.empty()) {
        ui->image_width_edit->setText("");
        ui->image_height_edit->setText("");
        ui->image_size_status_label->setText("No Data Selected");
        ui->image_size_status_label->setStyleSheet("color: #666666; font-style: italic;");
        std::cout << "Point_Widget::_updateImageSizeDisplay: No active key" << std::endl;
        return;
    }

    auto point_data = _data_manager->getData<PointData>(_active_key);
    if (!point_data) {
        ui->image_width_edit->setText("");
        ui->image_height_edit->setText("");
        ui->image_size_status_label->setText("Data Not Found");
        ui->image_size_status_label->setStyleSheet("color: #cc0000; font-style: italic;");
        std::cout << "Point_Widget::_updateImageSizeDisplay: No point data found for key: " << _active_key << std::endl;
        return;
    }

    ImageSize current_size = point_data->getImageSize();
    std::cout << "Point_Widget::_updateImageSizeDisplay: Current size: " << current_size.width << " x " << current_size.height << std::endl;
    
    if (current_size.width == -1 || current_size.height == -1) {
        ui->image_width_edit->setText("");
        ui->image_height_edit->setText("");
        ui->image_size_status_label->setText("Not Set");
        ui->image_size_status_label->setStyleSheet("color: #666666; font-style: italic;");
        std::cout << "Point_Widget::_updateImageSizeDisplay: No size set, clearing fields" << std::endl;
    } else {
        ui->image_width_edit->setText(QString::number(current_size.width));
        ui->image_height_edit->setText(QString::number(current_size.height));
        ui->image_size_status_label->setText(QString("%1 × %2").arg(current_size.width).arg(current_size.height));
        ui->image_size_status_label->setStyleSheet("color: #000000; font-weight: bold;");
        std::cout << "Point_Widget::_updateImageSizeDisplay: Set fields to " << current_size.width << " x " << current_size.height << std::endl;
    }
}

void Point_Widget::_onCopyImageSizeClicked() {
    if (_active_key.empty()) {
        QMessageBox::warning(this, "No Data Selected", "Please select a PointData item to modify image size.");
        return;
    }

    QString selected_media_key = ui->copy_from_media_combo->currentText();
    if (selected_media_key.isEmpty()) {
        QMessageBox::warning(this, "No Media Selected", "Please select a media source to copy image size from.");
        return;
    }

    auto media_data = _data_manager->getData<MediaData>(selected_media_key.toStdString());
    if (!media_data) {
        QMessageBox::critical(this, "Error", "Could not retrieve MediaData for key: " + selected_media_key);
        return;
    }

    ImageSize media_size = media_data->getImageSize();
    if (media_size.width == -1 || media_size.height == -1) {
        QMessageBox::warning(this, "No Image Size", 
            QString("The selected media '%1' does not have an image size set.").arg(selected_media_key));
        return;
    }

    auto point_data = _data_manager->getData<PointData>(_active_key);
    if (!point_data) {
        QMessageBox::critical(this, "Error", "Could not retrieve PointData for image size modification. Key: " + QString::fromStdString(_active_key));
        return;
    }

    // Get current image size
    ImageSize current_size = point_data->getImageSize();
    
    // If no current size is set, just set the new size without scaling
    if (current_size.width == -1 || current_size.height == -1) {
        point_data->setImageSize(media_size);
        _updateImageSizeDisplay();
        QMessageBox::information(this, "Image Size Set", 
            QString("Image size set to %1 × %2 (copied from '%3').")
                .arg(media_size.width).arg(media_size.height).arg(selected_media_key));
        return;
    }

    // Ask user if they want to scale existing data
    int ret = QMessageBox::question(this, "Scale Existing Data", 
        QString("Current image size is %1 × %2. Do you want to scale all existing point data to the new size %3 × %4 (from '%5')?\n\n"
               "Click 'Yes' to scale all point data proportionally.\n"
               "Click 'No' to just change the image size without scaling.\n"
               "Click 'Cancel' to abort the operation.")
            .arg(current_size.width).arg(current_size.height)
            .arg(media_size.width).arg(media_size.height).arg(selected_media_key),
        QMessageBox::Yes | QMessageBox::No | QMessageBox::Cancel);

    if (ret == QMessageBox::Cancel) {
        return;
    }

    if (ret == QMessageBox::Yes) {
        // Scale the data
        point_data->changeImageSize(media_size);
        QMessageBox::information(this, "Image Size Changed", 
            QString("Image size changed to %1 × %2 (copied from '%3') and all point data has been scaled proportionally.")
                .arg(media_size.width).arg(media_size.height).arg(selected_media_key));
    } else {
        // Just set the new size without scaling
        point_data->setImageSize(media_size);
        QMessageBox::information(this, "Image Size Set", 
            QString("Image size set to %1 × %2 (copied from '%3', existing point data was not scaled).")
                .arg(media_size.width).arg(media_size.height).arg(selected_media_key));
    }

    _updateImageSizeDisplay();
}

void Point_Widget::_populateMediaComboBox() {
    ui->copy_from_media_combo->clear();
    
    if (!_data_manager) {
        return;
    }

    // Get all MediaData keys
    auto media_keys = _data_manager->getKeys<MediaData>();
    
    if (media_keys.empty()) {
        ui->copy_from_media_combo->addItem("No media data available");
        ui->copy_from_media_combo->setEnabled(false);
        return;
    }

    ui->copy_from_media_combo->setEnabled(true);
    
    for (const auto& key : media_keys) {
        ui->copy_from_media_combo->addItem(QString::fromStdString(key));
    }
    
    std::cout << "Point_Widget::_populateMediaComboBox: Found " << media_keys.size() << " media keys" << std::endl;
}

void Point_Widget::setGroupManager(GroupManager * group_manager) {
    _group_manager = group_manager;
    _point_table_model->setGroupManager(group_manager);
    _populateGroupFilterCombo();
    
    // Connect to group manager signals to update when groups change
    if (_group_manager) {
        connect(_group_manager, &GroupManager::groupCreated,
                this, &Point_Widget::_onGroupChanged);
        connect(_group_manager, &GroupManager::groupRemoved,
                this, &Point_Widget::_onGroupChanged);
        connect(_group_manager, &GroupManager::groupModified,
                this, &Point_Widget::_onGroupChanged);
    }
}

void Point_Widget::_onGroupFilterChanged(int index) {
    if (!_group_manager) {
        return;
    }
    
    if (index == 0) {
        // "All Groups" selected
        _point_table_model->clearGroupFilter();
    } else {
        // Specific group selected (index - 1 because index 0 is "All Groups")
        auto groups = _group_manager->getGroups();
        auto group_ids = groups.keys();
        if (index - 1 < group_ids.size()) {
            int group_id = group_ids[index - 1];
            _point_table_model->setGroupFilter(group_id);
        }
    }
}

void Point_Widget::_onGroupChanged() {
    // Store current selection
    int current_index = ui->groupFilterCombo->currentIndex();
    
    // Update the group filter combo box when groups change
    _populateGroupFilterCombo();
    
    // If the previously selected group no longer exists, reset to "All Groups"
    if (current_index > 0 && current_index >= ui->groupFilterCombo->count()) {
        ui->groupFilterCombo->setCurrentIndex(0); // "All Groups"
        _point_table_model->clearGroupFilter();
    }
    
    // Refresh the table to update group names
    if (!_active_key.empty()) {
        updateTable();
    }
}

void Point_Widget::_populateGroupFilterCombo() {
    ui->groupFilterCombo->clear();
    ui->groupFilterCombo->addItem("All Groups");
    
    if (!_group_manager) {
        return;
    }
    
    auto groups = _group_manager->getGroups();
    for (auto it = groups.begin(); it != groups.end(); ++it) {
        ui->groupFilterCombo->addItem(it.value().name);
    }
}

void Point_Widget::_populateGroupSubmenu(QMenu * menu, bool for_moving) {
    if (!_group_manager) {
        return;
    }
    
    // Get current groups of selected entities to exclude them from the move list
    std::set<int> current_groups;
    if (for_moving) {
        QModelIndexList selectedIndexes = ui->tableView->selectionModel()->selectedRows();
        for (auto const & index : selectedIndexes) {
            PointTableRow const row_data = _point_table_model->getRowData(index.row());
            if (row_data.entity_id != 0) {
                int current_group = _group_manager->getEntityGroup(row_data.entity_id);
                if (current_group != -1) {
                    current_groups.insert(current_group);
                }
            }
        }
    }
    
    auto groups = _group_manager->getGroups();
    for (auto it = groups.begin(); it != groups.end(); ++it) {
        int group_id = it.key();
        QString group_name = it.value().name;
        
        // Skip current groups when moving
        if (for_moving && current_groups.find(group_id) != current_groups.end()) {
            continue;
        }
        
        QAction * action = menu->addAction(group_name);
        connect(action, &QAction::triggered, this, [this, group_id]() {
            _moveSelectedPointsToGroup(group_id);
        });
    }
}

void Point_Widget::_moveSelectedPointsToGroup(int group_id) {
    if (!_group_manager) {
        return;
    }
    
    // Get selected rows
    QModelIndexList selectedIndexes = ui->tableView->selectionModel()->selectedRows();
    if (selectedIndexes.isEmpty()) {
        return;
    }
    
    // Collect EntityIds from selected rows
    std::unordered_set<EntityId> entity_ids;
    for (auto const & index : selectedIndexes) {
        PointTableRow const row_data = _point_table_model->getRowData(index.row());
        if (row_data.entity_id != 0) { // Valid entity ID
            entity_ids.insert(row_data.entity_id);
        }
    }
    
    if (entity_ids.empty()) {
        return;
    }
    
    // First, remove entities from their current groups
    _group_manager->ungroupEntities(entity_ids);
    
    // Then, assign entities to the specified group
    _group_manager->assignEntitiesToGroup(group_id, entity_ids);
    
    // Refresh the table to show updated group information
    updateTable();
}

void Point_Widget::_removeSelectedPointsFromGroup() {
    if (!_group_manager) {
        return;
    }
    
    // Get selected rows
    QModelIndexList selectedIndexes = ui->tableView->selectionModel()->selectedRows();
    if (selectedIndexes.isEmpty()) {
        return;
    }
    
    // Collect EntityIds from selected rows
    std::unordered_set<EntityId> entity_ids;
    for (auto const & index : selectedIndexes) {
        PointTableRow const row_data = _point_table_model->getRowData(index.row());
        if (row_data.entity_id != 0) { // Valid entity ID
            entity_ids.insert(row_data.entity_id);
        }
    }
    
    if (entity_ids.empty()) {
        return;
    }
    
    // Remove entities from all groups
    _group_manager->ungroupEntities(entity_ids);
    
    // Refresh the table to show updated group information
    updateTable();
}
