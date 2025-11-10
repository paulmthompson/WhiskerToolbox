#include "Mask_Widget.hpp"
#include "ui_Mask_Widget.h"

#include "CoreGeometry/masks.hpp"
#include "CoreGeometry/points.hpp"
#include "CoreGeometry/ImageSize.hpp"
#include "DataManager/ConcreteDataFactory.hpp"
#include "DataManager/DataManager.hpp"
#include "DataManager/IO/LoaderRegistry.hpp"
#include "DataManager/Masks/Mask_Data.hpp"
#include "DataManager/Masks/utils/mask_utils.hpp"
#include "DataManager/Media/Media_Data.hpp"
#include "DataManager_Widget/utils/DataManager_Widget_utils.hpp"
#include "MaskTableModel.hpp"
#include "WhiskerToolbox/GroupManagementWidget/GroupManager.hpp"

#include "utils/Deep_Learning/Models/EfficientSAM/EfficientSAM.hpp"

#include "IO_Widgets/Masks/HDF5/HDF5MaskSaver_Widget.hpp"
#include "IO_Widgets/Masks/Image/ImageMaskSaver_Widget.hpp"
#include "MediaExport/MediaExport_Widget.hpp"
#include "MediaExport/media_export.hpp"

#include <QCheckBox>
#include <QComboBox>
#include <QDir>
#include <QFileDialog>
#include <QLineEdit>
#include <QMenu>
#include <QMessageBox>
#include <QPushButton>
#include <QStackedWidget>
#include <QString>
#include <QTableView>
#include <QModelIndex>

#include <filesystem>
#include <iostream>
#include <set>
#include <unordered_set>

Mask_Widget::Mask_Widget(std::shared_ptr<DataManager> data_manager, QWidget * parent)
    : QWidget(parent),
      ui(new Ui::Mask_Widget),
      _data_manager{std::move(data_manager)},
      _mask_table_model{std::make_unique<MaskTableModel>()} {
    ui->setupUi(this);

    ui->tableView->setModel(_mask_table_model.get());
    ui->tableView->setSelectionBehavior(QAbstractItemView::SelectRows);
    ui->tableView->setSelectionMode(QAbstractItemView::ExtendedSelection);
    ui->tableView->setEditTriggers(QAbstractItemView::NoEditTriggers);
    ui->tableView->setContextMenuPolicy(Qt::CustomContextMenu);

    connect(ui->load_sam_button, &QPushButton::clicked, this, &Mask_Widget::_loadSamModel);
    connect(ui->tableView, &QTableView::doubleClicked, this, &Mask_Widget::_handleTableViewDoubleClicked);
    connect(ui->tableView, &QTableView::customContextMenuRequested, this, &Mask_Widget::_showContextMenu);

    // Connect export functionality
    connect(ui->export_type_combo, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &Mask_Widget::_onExportTypeChanged);
    connect(ui->image_mask_saver_widget, &ImageMaskSaver_Widget::saveImageMaskRequested,
            this, &Mask_Widget::_handleSaveImageMaskRequested);
    connect(ui->export_media_frames_checkbox, &QCheckBox::toggled,
            this, &Mask_Widget::_onExportMediaFramesCheckboxToggled);
    connect(ui->apply_image_size_button, &QPushButton::clicked,
            this, &Mask_Widget::_onApplyImageSizeClicked);
    connect(ui->copy_image_size_button, &QPushButton::clicked,
            this, &Mask_Widget::_onCopyImageSizeClicked);
    connect(ui->groupFilterCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &Mask_Widget::_onGroupFilterChanged);

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

Mask_Widget::~Mask_Widget() {
    delete ui;
}

void Mask_Widget::openWidget() {
    this->show();
    // Update image size display when widget is opened
    _updateImageSizeDisplay();
}

void Mask_Widget::setActiveKey(std::string const & key) {
    if (_active_key == key && _callback_id != -1) {
        updateTable();
        return;
    }
    removeCallbacks();

    _active_key = key;
    updateTable();

    if (!_active_key.empty()) {
        auto mask_data = _data_manager->getData<MaskData>(_active_key);
        if (mask_data) {
            _callback_id = _data_manager->addCallbackToData(_active_key, [this]() { _onDataChanged(); });
            _updateImageSizeDisplay();
        } else {
            std::cerr << "Mask_Widget: No MaskData found for key '" << _active_key << "' to attach callback." << std::endl;
        }
    }
}

void Mask_Widget::updateTable() {
    if (!_active_key.empty()) {
        auto mask_data = _data_manager->getData<MaskData>(_active_key);
        _mask_table_model->setMasks(mask_data.get());
    } else {
        _mask_table_model->setMasks(nullptr);
    }
    _updateImageSizeDisplay();
}

void Mask_Widget::removeCallbacks() {
    remove_callback(_data_manager.get(), _active_key, _callback_id);
}

void Mask_Widget::_onDataChanged() {
    updateTable();
}

void Mask_Widget::_handleTableViewDoubleClicked(QModelIndex const & index) {
    if (!index.isValid()) return;
    int const frame = _mask_table_model->getFrameForRow(index.row());
    if (frame != -1) {
        emit frameSelected(frame);
    }
}

std::vector<TimeFrameIndex> Mask_Widget::_getSelectedFrames() {
    std::vector<TimeFrameIndex> selected_frames;
    QModelIndexList const selected_rows = ui->tableView->selectionModel()->selectedRows();

    for (QModelIndex const & index: selected_rows) {
        if (index.isValid()) {
            int const frame = _mask_table_model->getFrameForRow(index.row());
            if (frame != -1) {
                selected_frames.emplace_back(frame);
            }
        }
    }

    return selected_frames;
}

std::vector<EntityId> Mask_Widget::_getSelectedEntityIds() {
    QModelIndexList const selectedIndexes = ui->tableView->selectionModel()->selectedRows();
    std::vector<EntityId> entity_ids;

    for (auto const & index: selectedIndexes) {
        if (index.isValid()) {
            MaskTableRow const row_data = _mask_table_model->getRowData(index.row());
            if (row_data.entity_id != EntityId(0)) { // Valid entity ID
                entity_ids.push_back(row_data.entity_id);
            }
        }
    }

    return entity_ids;
}

void Mask_Widget::_showContextMenu(QPoint const & position) {
    QModelIndex const index = ui->tableView->indexAt(position);
    if (!index.isValid()) {
        return;
    }

    QMenu context_menu(this);

    // Add move and copy submenus using the utility function
    auto move_callback = [this](std::string const & target_key) {
        _moveMasksToTarget(target_key);
    };

    auto copy_callback = [this](std::string const & target_key) {
        _copyMasksToTarget(target_key);
    };

    add_move_copy_submenus<MaskData>(&context_menu, _data_manager.get(), _active_key, move_callback, copy_callback);

    // Add group management options
    context_menu.addSeparator();
    QMenu * group_menu = context_menu.addMenu("Group Management");
    
    // Add "Move to Group" submenu
    QMenu * move_to_group_menu = group_menu->addMenu("Move to Group");
    _populateGroupSubmenu(move_to_group_menu, true);
    
    // Add "Remove from Group" action
    QAction * remove_from_group_action = group_menu->addAction("Remove from Group");
    connect(remove_from_group_action, &QAction::triggered, this, &Mask_Widget::_removeSelectedMasksFromGroup);

    // Add separator and delete operation
    context_menu.addSeparator();
    QAction * delete_action = context_menu.addAction("Delete Selected Masks");

    connect(delete_action, &QAction::triggered, this, &Mask_Widget::_deleteSelectedMasks);

    context_menu.exec(ui->tableView->mapToGlobal(position));
}

void Mask_Widget::_moveMasksToTarget(std::string const & target_key) {
    auto selected_entity_ids = _getSelectedEntityIds();
    if (selected_entity_ids.empty()) {
        std::cout << "Mask_Widget: No masks selected to move." << std::endl;
        return;
    }

    auto source_mask_data = _data_manager->getData<MaskData>(_active_key);
    auto target_mask_data = _data_manager->getData<MaskData>(target_key);

    if (!source_mask_data) {
        std::cerr << "Mask_Widget: Source MaskData object ('" << _active_key << "') not found." << std::endl;
        return;
    }
    if (!target_mask_data) {
        std::cerr << "Mask_Widget: Target MaskData object ('" << target_key << "') not found." << std::endl;
        return;
    }

    std::cout << "Mask_Widget: Moving " << selected_entity_ids.size()
              << " selected masks from '" << _active_key << "' to '" << target_key << "'..." << std::endl;

    // Use the moveByEntityIds method to move only the selected masks
    std::unordered_set<EntityId> const selected_entity_ids_set(selected_entity_ids.begin(), selected_entity_ids.end());
    std::size_t const total_masks_moved = source_mask_data->moveByEntityIds(*target_mask_data,
                                                                            selected_entity_ids_set,
                                                                            true);

    if (total_masks_moved > 0) {
        // Update the table view to reflect changes
        updateTable();

        std::cout << "Mask_Widget: Successfully moved " << total_masks_moved
                  << " selected masks." << std::endl;
    } else {
        std::cout << "Mask_Widget: No masks found with the selected EntityIds to move." << std::endl;
    }
}

void Mask_Widget::_copyMasksToTarget(std::string const & target_key) {
    auto selected_entity_ids = _getSelectedEntityIds();
    if (selected_entity_ids.empty()) {
        std::cout << "Mask_Widget: No masks selected to copy." << std::endl;
        return;
    }

    auto source_mask_data = _data_manager->getData<MaskData>(_active_key);
    auto target_mask_data = _data_manager->getData<MaskData>(target_key);

    if (!source_mask_data) {
        std::cerr << "Mask_Widget: Source MaskData object ('" << _active_key << "') not found." << std::endl;
        return;
    }
    if (!target_mask_data) {
        std::cerr << "Mask_Widget: Target MaskData object ('" << target_key << "') not found." << std::endl;
        return;
    }

    std::cout << "Mask_Widget: Copying " << selected_entity_ids.size()
              << " selected masks from '" << _active_key << "' to '" << target_key << "'..." << std::endl;

    // Use the copyByEntityIds method to copy only the selected masks
    std::unordered_set<EntityId> const selected_entity_ids_set(selected_entity_ids.begin(), selected_entity_ids.end());
    std::size_t const total_masks_copied = source_mask_data->copyByEntityIds(*target_mask_data,
                                                                              selected_entity_ids_set,
                                                                              true);

    if (total_masks_copied > 0) {
        std::cout << "Mask_Widget: Successfully copied " << total_masks_copied
                  << " selected masks." << std::endl;
    } else {
        std::cout << "Mask_Widget: No masks found with the selected EntityIds to copy." << std::endl;
    }
}

void Mask_Widget::_deleteSelectedMasks() {
    QModelIndexList selectedIndexes = ui->tableView->selectionModel()->selectedRows();
    if (selectedIndexes.isEmpty()) {
        std::cout << "Mask_Widget: No masks selected to delete." << std::endl;
        return;
    }

    auto mask_data_ptr = _data_manager->getData<MaskData>(_active_key);
    if (!mask_data_ptr) {
        std::cerr << "Mask_Widget: Source MaskData object ('" << _active_key << "') not found for deletion." << std::endl;
        return;
    }

    std::cout << "Mask_Widget: Deleting " << selectedIndexes.size()
              << " selected masks from '" << _active_key << "'..." << std::endl;

    int total_masks_deleted = 0;

    // Delete each selected mask individually
    for (auto const & index: selectedIndexes) {
        if (index.isValid()) {
            MaskTableRow const row_data = _mask_table_model->getRowData(index.row());
            if (row_data.frame != -1 && row_data.entity_id != EntityId(0)) {
                bool const success = mask_data_ptr->clearByEntityId(row_data.entity_id, NotifyObservers::No);
                if (success) {
                    total_masks_deleted++;
                }
            }
        }
    }

    // Notify observers only once at the end
    if (total_masks_deleted > 0) {
        mask_data_ptr->notifyObservers();

        // Update the table view to reflect changes
        updateTable();

        std::cout << "Mask_Widget: Successfully deleted " << total_masks_deleted
                  << " selected masks." << std::endl;
    } else {
        std::cout << "Mask_Widget: No masks found to delete from the selected rows." << std::endl;
    }
}

void Mask_Widget::selectPoint(float const x, float const y) {
    auto media = _data_manager->getData<MediaData>("media");
    if (!media) {
        std::cerr << "Mask_Widget: Media data not found." << std::endl;
        return;
    }
    if (!_sam_model) {
        std::cerr << "Mask_Widget: SAM model not loaded." << std::endl;
        return;
    }

    auto current_time = _data_manager->getCurrentTime();
    auto const image_size = media->getImageSize();
    if (image_size.width <= 0 || image_size.height <= 0) {
        std::cerr << "Mask_Widget: Invalid image size from media." << std::endl;
        return;
    }

    auto processed_frame_data = media->getProcessedData8(current_time);
    if (processed_frame_data.empty()) {
        std::cerr << "Mask_Widget: Processed frame data is empty." << std::endl;
        return;
    }

    auto mask_image = _sam_model->process_frame(
            processed_frame_data,
            image_size,
            static_cast<int>(std::round(x)),
            static_cast<int>(std::round(y)));

    std::vector<Point2D<uint32_t>> mask;
    for (size_t i = 0; i < mask_image.size(); i++) {
        if (mask_image[i] > 0) {
            mask.emplace_back(static_cast<uint32_t>(i % image_size.width),
                              static_cast<uint32_t>(i / image_size.width));
        }
    }
    if (_active_key.empty()) {
        std::cerr << "Mask_Widget: No active key set for MaskData." << std::endl;
        return;
    }
    auto active_mask_data = _data_manager->getData<MaskData>(_active_key);
    if (!active_mask_data) {
        std::cerr << "Mask_Widget: Active MaskData object not found for key: " << _active_key << std::endl;
        return;
    }

    active_mask_data->addAtTime(TimeFrameIndex(current_time), mask);
}

void Mask_Widget::_loadSamModel() {
    _sam_model = std::make_unique<dl::EfficientSAM>();
    // Assuming load_model handles its own error reporting or exceptions
    _sam_model->load_model();
    // Potentially update UI to indicate model is loaded, e.g., enable/disable buttons
    /*
    if(_sam_model->is_loaded()){
        std::cout << "Mask_Widget: SAM model loaded successfully." << std::endl;
    } else {
        std::cout << "Mask_Widget: SAM model failed to load." << std::endl;
    }
    */
}

void Mask_Widget::_onExportTypeChanged(int index) {
    QString const current_text = ui->export_type_combo->itemText(index);
    if (current_text == "HDF5") {
        ui->stacked_saver_options->setCurrentWidget(ui->hdf5_mask_saver_widget);
    } else if (current_text == "Image") {
        ui->stacked_saver_options->setCurrentWidget(ui->image_mask_saver_widget);
    }
}

void Mask_Widget::_handleSaveImageMaskRequested(QString format, nlohmann::json config) {
    _initiateSaveProcess(format, config);
}

void Mask_Widget::_onExportMediaFramesCheckboxToggled(bool checked) {
    ui->media_export_options_widget->setVisible(checked);
}

void Mask_Widget::_initiateSaveProcess(QString const & format, MaskSaverConfig const & config) {
    if (_active_key.empty()) {
        QMessageBox::warning(this, "No Data Selected", "Please select a MaskData item to save.");
        return;
    }

    auto mask_data_ptr = _data_manager->getData<MaskData>(_active_key);
    if (!mask_data_ptr) {
        QMessageBox::critical(this, "Error", "Could not retrieve MaskData for saving. Key: " + QString::fromStdString(_active_key));
        return;
    }

    // Update config with full path
    MaskSaverConfig updated_config = config;
    std::string const parent_dir = config.value("parent_dir", ".");
    updated_config["parent_dir"] = _data_manager->getOutputPath().string() + "/" + parent_dir;

    bool const save_successful = _performRegistrySave(format, updated_config);

    if (!save_successful) {
        return;
    }

    if (ui->export_media_frames_checkbox->isChecked()) {
        auto times_with_data = mask_data_ptr->getTimesWithData();
        std::vector<size_t> frame_ids_to_export;
        frame_ids_to_export.reserve(times_with_data.size());
        for (auto frame_id: times_with_data) {
            frame_ids_to_export.push_back(static_cast<size_t>(frame_id.getValue()));
        }

        if (frame_ids_to_export.empty()) {
            QMessageBox::information(this, "No Frames", "No masks found in data, so no media frames to export.");
        } else {
            auto media_ptr = _data_manager->getData<MediaData>("media");
            if (!media_ptr) {
                QMessageBox::warning(this, "Media Not Available", "Could not access media for exporting frames.");
                return;
            }

            MediaExportOptions options = ui->media_export_options_widget->getOptions();
            // Use the same parent directory used for mask saving
            std::string const base_output_dir = updated_config.value("parent_dir", _data_manager->getOutputPath().string());
            options.image_save_dir = base_output_dir;

            try {
                std::filesystem::create_directories(options.image_save_dir);
            } catch (std::exception const & e) {
                QMessageBox::critical(this, "Export Error", QString("Failed to create output directory: %1\n%2").arg(QString::fromStdString(options.image_save_dir)).arg(QString::fromStdString(e.what())));
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
    }
}

bool Mask_Widget::_performRegistrySave(QString const & format, MaskSaverConfig const & config) {
    auto mask_data_ptr = _data_manager->getData<MaskData>(_active_key);
    if (!mask_data_ptr) {
        QMessageBox::critical(this, "Save Error", "Critical: Could not retrieve MaskData for saving. Key: " + QString::fromStdString(_active_key));
        return false;
    }

    // Check if format is supported through registry
    LoaderRegistry & registry = LoaderRegistry::getInstance();
    std::string format_str = format.toStdString();

    if (!registry.isFormatSupported(format_str, IODataType::Mask)) {
        QMessageBox::warning(this, "Format Not Supported",
                             QString("Format '%1' saving is not available. This may require additional plugins to be enabled.\n\n"
                                     "To enable format support:\n"
                                     "1. Ensure required libraries are available in your build environment\n"
                                     "2. Build with appropriate -DENABLE_* flags\n"
                                     "3. Restart the application")
                                     .arg(format));

        std::cout << "Format '" << format_str << "' saving not available - plugin not registered" << std::endl;
        return false;
    }

    try {
        // Use registry to save through the new save interface
        LoadResult result = registry.trySave(format_str,
                                             IODataType::Mask,
                                             "",// filepath not used for directory-based saving
                                             config,
                                             mask_data_ptr.get());

        if (result.success) {
            std::string save_location = config.value("parent_dir", ".");
            QMessageBox::information(this, "Save Successful",
                                     QString("Mask data saved successfully to: %1").arg(QString::fromStdString(save_location)));
            std::cout << "Mask data saved successfully using " << format_str << " format" << std::endl;
            return true;
        } else {
            QMessageBox::critical(this, "Save Error",
                                  QString("Failed to save mask data: %1").arg(QString::fromStdString(result.error_message)));
            std::cerr << "Failed to save mask data: " << result.error_message << std::endl;
            return false;
        }

    } catch (std::exception const & e) {
        QMessageBox::critical(this, "Save Error", "Failed to save mask data: " + QString::fromStdString(e.what()));
        std::cerr << "Failed to save mask data: " << e.what() << std::endl;
        return false;
    }
}

void Mask_Widget::_updateImageSizeDisplay() {
    if (_active_key.empty()) {
        ui->image_width_edit->setText("");
        ui->image_height_edit->setText("");
        ui->image_size_status_label->setText("No Data Selected");
        ui->image_size_status_label->setStyleSheet("color: #666666; font-style: italic;");
        std::cout << "Mask_Widget::_updateImageSizeDisplay: No active key" << std::endl;
        return;
    }

    auto mask_data = _data_manager->getData<MaskData>(_active_key);
    if (!mask_data) {
        ui->image_width_edit->setText("");
        ui->image_height_edit->setText("");
        ui->image_size_status_label->setText("Data Not Found");
        ui->image_size_status_label->setStyleSheet("color: #cc0000; font-style: italic;");
        std::cout << "Mask_Widget::_updateImageSizeDisplay: No mask data found for key: " << _active_key << std::endl;
        return;
    }

    ImageSize current_size = mask_data->getImageSize();
    std::cout << "Mask_Widget::_updateImageSizeDisplay: Current size: " << current_size.width << " x " << current_size.height << std::endl;
    
    if (current_size.width == -1 || current_size.height == -1) {
        ui->image_width_edit->setText("");
        ui->image_height_edit->setText("");
        ui->image_size_status_label->setText("Not Set");
        ui->image_size_status_label->setStyleSheet("color: #666666; font-style: italic;");
        std::cout << "Mask_Widget::_updateImageSizeDisplay: No size set, clearing fields" << std::endl;
    } else {
        ui->image_width_edit->setText(QString::number(current_size.width));
        ui->image_height_edit->setText(QString::number(current_size.height));
        ui->image_size_status_label->setText(QString("%1 × %2").arg(current_size.width).arg(current_size.height));
        ui->image_size_status_label->setStyleSheet("color: #000000; font-weight: bold;");
        std::cout << "Mask_Widget::_updateImageSizeDisplay: Set fields to " << current_size.width << " x " << current_size.height << std::endl;
    }
}

void Mask_Widget::_onApplyImageSizeClicked() {
    if (_active_key.empty()) {
        QMessageBox::warning(this, "No Data Selected", "Please select a MaskData item to modify image size.");
        return;
    }

    auto mask_data = _data_manager->getData<MaskData>(_active_key);
    if (!mask_data) {
        QMessageBox::critical(this, "Error", "Could not retrieve MaskData for image size modification. Key: " + QString::fromStdString(_active_key));
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
    ImageSize current_size = mask_data->getImageSize();
    
    // If no current size is set, just set the new size without scaling
    if (current_size.width == -1 || current_size.height == -1) {
        mask_data->setImageSize({new_width, new_height});
        _updateImageSizeDisplay();
        QMessageBox::information(this, "Image Size Set", 
            QString("Image size set to %1 × %2 (no scaling applied as no previous size was set).")
                .arg(new_width).arg(new_height));
        return;
    }

    // Ask user if they want to scale existing data
    int ret = QMessageBox::question(this, "Scale Existing Data", 
        QString("Current image size is %1 × %2. Do you want to scale all existing mask data to the new size %3 × %4?\n\n"
               "Click 'Yes' to scale all mask data proportionally.\n"
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
        mask_data->changeImageSize({new_width, new_height});
        QMessageBox::information(this, "Image Size Changed", 
            QString("Image size changed to %1 × %2 and all mask data has been scaled proportionally.")
                .arg(new_width).arg(new_height));
    } else {
        // Just set the new size without scaling
        mask_data->setImageSize({new_width, new_height});
        QMessageBox::information(this, "Image Size Set", 
            QString("Image size set to %1 × %2 (existing mask data was not scaled).")
                .arg(new_width).arg(new_height));
    }

    _updateImageSizeDisplay();
}

void Mask_Widget::_onCopyImageSizeClicked() {
    if (_active_key.empty()) {
        QMessageBox::warning(this, "No Data Selected", "Please select a MaskData item to modify image size.");
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

    auto mask_data = _data_manager->getData<MaskData>(_active_key);
    if (!mask_data) {
        QMessageBox::critical(this, "Error", "Could not retrieve MaskData for image size modification. Key: " + QString::fromStdString(_active_key));
        return;
    }

    // Get current image size
    ImageSize current_size = mask_data->getImageSize();
    
    // If no current size is set, just set the new size without scaling
    if (current_size.width == -1 || current_size.height == -1) {
        mask_data->setImageSize(media_size);
        _updateImageSizeDisplay();
        QMessageBox::information(this, "Image Size Set", 
            QString("Image size set to %1 × %2 (copied from '%3').")
                .arg(media_size.width).arg(media_size.height).arg(selected_media_key));
        return;
    }

    // Ask user if they want to scale existing data
    int ret = QMessageBox::question(this, "Scale Existing Data", 
        QString("Current image size is %1 × %2. Do you want to scale all existing mask data to the new size %3 × %4 (from '%5')?\n\n"
               "Click 'Yes' to scale all mask data proportionally.\n"
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
        mask_data->changeImageSize(media_size);
        QMessageBox::information(this, "Image Size Changed", 
            QString("Image size changed to %1 × %2 (copied from '%3') and all mask data has been scaled proportionally.")
                .arg(media_size.width).arg(media_size.height).arg(selected_media_key));
    } else {
        // Just set the new size without scaling
        mask_data->setImageSize(media_size);
        QMessageBox::information(this, "Image Size Set", 
            QString("Image size set to %1 × %2 (copied from '%3', existing mask data was not scaled).")
                .arg(media_size.width).arg(media_size.height).arg(selected_media_key));
    }

    _updateImageSizeDisplay();
}

void Mask_Widget::_populateMediaComboBox() {
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
    
    std::cout << "Mask_Widget::_populateMediaComboBox: Found " << media_keys.size() << " media keys" << std::endl;
}

void Mask_Widget::setGroupManager(GroupManager * group_manager) {
    _group_manager = group_manager;
    _mask_table_model->setGroupManager(group_manager);
    _populateGroupFilterCombo();
    
    // Connect to group manager signals to update when groups change
    if (_group_manager) {
        connect(_group_manager, &GroupManager::groupCreated,
                this, &Mask_Widget::_onGroupChanged);
        connect(_group_manager, &GroupManager::groupRemoved,
                this, &Mask_Widget::_onGroupChanged);
        connect(_group_manager, &GroupManager::groupModified,
                this, &Mask_Widget::_onGroupChanged);
    }
}

void Mask_Widget::_onGroupFilterChanged(int index) {
    if (!_group_manager) {
        return;
    }
    
    if (index == 0) {
        // "All Groups" selected
        _mask_table_model->clearGroupFilter();
    } else {
        // Specific group selected (index - 1 because index 0 is "All Groups")
        auto groups = _group_manager->getGroups();
        auto group_ids = groups.keys();
        if (index - 1 < group_ids.size()) {
            int group_id = group_ids[index - 1];
            _mask_table_model->setGroupFilter(group_id);
        }
    }
}

void Mask_Widget::_onGroupChanged() {
    // Store current selection
    int current_index = ui->groupFilterCombo->currentIndex();
    
    // Update the group filter combo box when groups change
    _populateGroupFilterCombo();
    
    // If the previously selected group no longer exists, reset to "All Groups"
    if (current_index > 0 && current_index >= ui->groupFilterCombo->count()) {
        ui->groupFilterCombo->setCurrentIndex(0); // "All Groups"
        _mask_table_model->clearGroupFilter();
    }
    
    // Refresh the table to update group names
    if (!_active_key.empty()) {
        updateTable();
    }
}

void Mask_Widget::_populateGroupFilterCombo() {
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

void Mask_Widget::_populateGroupSubmenu(QMenu * menu, bool for_moving) {
    if (!_group_manager) {
        return;
    }
    
    // Get current groups of selected entities to exclude them from the move list
    std::set<int> current_groups;
    if (for_moving) {
        QModelIndexList selectedIndexes = ui->tableView->selectionModel()->selectedRows();
        for (auto const & index : selectedIndexes) {
            MaskTableRow const row_data = _mask_table_model->getRowData(index.row());
            if (row_data.entity_id != EntityId(0)) {
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
            _moveSelectedMasksToGroup(group_id);
        });
    }
}

void Mask_Widget::_moveSelectedMasksToGroup(int group_id) {
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
        MaskTableRow const row_data = _mask_table_model->getRowData(index.row());
        if (row_data.entity_id != EntityId(0)) { // Valid entity ID
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

void Mask_Widget::_removeSelectedMasksFromGroup() {
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
        MaskTableRow const row_data = _mask_table_model->getRowData(index.row());
        if (row_data.entity_id != EntityId(0)) { // Valid entity ID
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
