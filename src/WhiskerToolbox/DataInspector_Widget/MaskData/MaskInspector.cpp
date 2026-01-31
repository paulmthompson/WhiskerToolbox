#include "MaskInspector.hpp"
#include "ui_MaskInspector.h"

#include "MaskTableView.hpp"
#include "DataInspector_Widget/Inspectors/GroupFilterHelper.hpp"

#include "CoreGeometry/ImageSize.hpp"
#include "DataManager/DataManager.hpp"
#include "DataManager/IO/core/LoaderRegistry.hpp"
#include "DataManager/Masks/Mask_Data.hpp"
#include "DataManager/Media/Media_Data.hpp"
#include "Entity/EntityTypes.hpp"
#include "MediaExport/media_export.hpp"
#include "WhiskerToolbox/GroupManagementWidget/GroupManager.hpp"

#include <QCheckBox>
#include <QComboBox>
#include <QMessageBox>
#include <QPushButton>
#include <QStackedWidget>

#include <filesystem>
#include <iostream>
#include <unordered_set>

MaskInspector::MaskInspector(
    std::shared_ptr<DataManager> data_manager,
    GroupManager * group_manager,
    QWidget * parent)
    : BaseInspector(std::move(data_manager), group_manager, parent),
      ui(new Ui::MaskInspector) {
    ui->setupUi(this);
    _connectSignals();
    
    // Populate media combo box (after UI is set up)
    _populateMediaComboBox();
    
    // Set up callback to refresh media combo box when data changes
    if (dataManager()) {
        _dm_observer_id = dataManager()->addObserver([this]() {
            _populateMediaComboBox();
        });
    }

    // Initialize group filter combo box
    _populateGroupFilterCombo();
}

MaskInspector::~MaskInspector() {
    removeCallbacks();
    // Remove DataManager-level observer
    if (dataManager() && _dm_observer_id != -1) {
        dataManager()->removeObserver(_dm_observer_id);
    }
    delete ui;
}

void MaskInspector::setActiveKey(std::string const & key) {
    if (_active_key == key && _callback_id != -1) {
        _updateImageSizeDisplay();
        return;
    }
    removeCallbacks();

    _active_key = key;
    _updateImageSizeDisplay();

    if (!_active_key.empty()) {
        auto mask_data = dataManager()->getData<MaskData>(_active_key);
        if (mask_data) {
            _callback_id = dataManager()->addCallbackToData(_active_key, [this]() { _onDataChanged(); });
            _updateImageSizeDisplay();
        } else {
            std::cerr << "MaskInspector: No MaskData found for key '" << _active_key << "' to attach callback." << std::endl;
        }
    }
}

void MaskInspector::removeCallbacks() {
    removeCallbackFromData(_active_key, _callback_id);
}

void MaskInspector::updateView() {
    _updateImageSizeDisplay();
}

void MaskInspector::_connectSignals() {
    connect(ui->load_sam_button, &QPushButton::clicked, this, &MaskInspector::_loadSamModel);

    // Connect export functionality
    connect(ui->export_type_combo, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &MaskInspector::_onExportTypeChanged);
    connect(ui->image_mask_saver_widget, &ImageMaskSaver_Widget::saveImageMaskRequested,
            this, &MaskInspector::_handleSaveImageMaskRequested);
    connect(ui->export_media_frames_checkbox, &QCheckBox::toggled,
            this, &MaskInspector::_onExportMediaFramesCheckboxToggled);
    connect(ui->apply_image_size_button, &QPushButton::clicked,
            this, &MaskInspector::_onApplyImageSizeClicked);
    connect(ui->copy_image_size_button, &QPushButton::clicked,
            this, &MaskInspector::_onCopyImageSizeClicked);

    // Group filter signals
    connect(ui->groupFilterCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &MaskInspector::_onGroupFilterChanged);

    // Group manager signals
    connectGroupManagerSignals(groupManager(), this, &MaskInspector::_onGroupChanged);

    // Setup collapsible export section
    ui->export_section->autoSetContentLayout();
    ui->export_section->setTitle("Export Options");
    ui->export_section->toggle(false); // Start collapsed

    _onExportTypeChanged(ui->export_type_combo->currentIndex());
    ui->media_export_options_widget->setVisible(ui->export_media_frames_checkbox->isChecked());
}

void MaskInspector::_onDataChanged() {
    _updateImageSizeDisplay();
}

void MaskInspector::_loadSamModel() {
    // SAM model loading functionality (currently disabled)
}

void MaskInspector::_onExportTypeChanged(int index) {
    QString const current_text = ui->export_type_combo->itemText(index);
    if (current_text == "HDF5") {
        ui->stacked_saver_options->setCurrentWidget(ui->hdf5_mask_saver_widget);
    } else if (current_text == "Image") {
        ui->stacked_saver_options->setCurrentWidget(ui->image_mask_saver_widget);
    }
}

void MaskInspector::_handleSaveImageMaskRequested(QString format, nlohmann::json config) {
    _initiateSaveProcess(format, config);
}

void MaskInspector::_onExportMediaFramesCheckboxToggled(bool checked) {
    ui->media_export_options_widget->setVisible(checked);
}

void MaskInspector::_initiateSaveProcess(QString const & format, MaskSaverConfig const & config) {
    if (_active_key.empty()) {
        QMessageBox::warning(this, "No Data Selected", "Please select a MaskData item to save.");
        return;
    }

    auto mask_data_ptr = dataManager()->getData<MaskData>(_active_key);
    if (!mask_data_ptr) {
        QMessageBox::critical(this, "Error", "Could not retrieve MaskData for saving. Key: " + QString::fromStdString(_active_key));
        return;
    }

    // Update config with full path
    MaskSaverConfig updated_config = config;
    std::string const parent_dir = config.value("parent_dir", ".");
    updated_config["parent_dir"] = dataManager()->getOutputPath() + "/" + parent_dir;

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
            auto media_ptr = dataManager()->getData<MediaData>("media");
            if (!media_ptr) {
                QMessageBox::warning(this, "Media Not Available", "Could not access media for exporting frames.");
                return;
            }

            MediaExportOptions options = ui->media_export_options_widget->getOptions();
            // Use the same parent directory used for mask saving
            std::string const base_output_dir = updated_config.value("parent_dir", dataManager()->getOutputPath());
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

bool MaskInspector::_performRegistrySave(QString const & format, MaskSaverConfig const & config) {
    auto mask_data_ptr = dataManager()->getData<MaskData>(_active_key);
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

void MaskInspector::_updateImageSizeDisplay() {
    if (_active_key.empty()) {
        ui->image_width_edit->setText("");
        ui->image_height_edit->setText("");
        ui->image_size_status_label->setText("No Data Selected");
        ui->image_size_status_label->setStyleSheet("color: #666666; font-style: italic;");
        std::cout << "MaskInspector::_updateImageSizeDisplay: No active key" << std::endl;
        return;
    }

    auto mask_data = dataManager()->getData<MaskData>(_active_key);
    if (!mask_data) {
        ui->image_width_edit->setText("");
        ui->image_height_edit->setText("");
        ui->image_size_status_label->setText("Data Not Found");
        ui->image_size_status_label->setStyleSheet("color: #cc0000; font-style: italic;");
        std::cout << "MaskInspector::_updateImageSizeDisplay: No mask data found for key: " << _active_key << std::endl;
        return;
    }

    ImageSize current_size = mask_data->getImageSize();
    std::cout << "MaskInspector::_updateImageSizeDisplay: Current size: " << current_size.width << " x " << current_size.height << std::endl;
    
    if (current_size.width == -1 || current_size.height == -1) {
        ui->image_width_edit->setText("");
        ui->image_height_edit->setText("");
        ui->image_size_status_label->setText("Not Set");
        ui->image_size_status_label->setStyleSheet("color: #666666; font-style: italic;");
        std::cout << "MaskInspector::_updateImageSizeDisplay: No size set, clearing fields" << std::endl;
    } else {
        ui->image_width_edit->setText(QString::number(current_size.width));
        ui->image_height_edit->setText(QString::number(current_size.height));
        ui->image_size_status_label->setText(QString("%1 × %2").arg(current_size.width).arg(current_size.height));
        ui->image_size_status_label->setStyleSheet("color: #000000; font-weight: bold;");
        std::cout << "MaskInspector::_updateImageSizeDisplay: Set fields to " << current_size.width << " x " << current_size.height << std::endl;
    }
}

void MaskInspector::_onApplyImageSizeClicked() {
    if (_active_key.empty()) {
        QMessageBox::warning(this, "No Data Selected", "Please select a MaskData item to modify image size.");
        return;
    }

    auto mask_data = dataManager()->getData<MaskData>(_active_key);
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

void MaskInspector::_onCopyImageSizeClicked() {
    if (_active_key.empty()) {
        QMessageBox::warning(this, "No Data Selected", "Please select a MaskData item to modify image size.");
        return;
    }

    QString selected_media_key = ui->copy_from_media_combo->currentText();
    if (selected_media_key.isEmpty()) {
        QMessageBox::warning(this, "No Media Selected", "Please select a media source to copy image size from.");
        return;
    }

    auto media_data = dataManager()->getData<MediaData>(selected_media_key.toStdString());
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

    auto mask_data = dataManager()->getData<MaskData>(_active_key);
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

void MaskInspector::_populateMediaComboBox() {
    ui->copy_from_media_combo->clear();
    
    if (!dataManager()) {
        return;
    }

    // Get all MediaData keys
    auto media_keys = dataManager()->getKeys<MediaData>();
    
    if (media_keys.empty()) {
        ui->copy_from_media_combo->addItem("No media data available");
        ui->copy_from_media_combo->setEnabled(false);
        return;
    }

    ui->copy_from_media_combo->setEnabled(true);
    
    for (const auto& key : media_keys) {
        ui->copy_from_media_combo->addItem(QString::fromStdString(key));
    }
    
    std::cout << "MaskInspector::_populateMediaComboBox: Found " << media_keys.size() << " media keys" << std::endl;
}

void MaskInspector::setDataView(MaskTableView * view) {
    // Disconnect from old view if any
    if (_data_view) {
        disconnect(_data_view, &MaskTableView::moveMasksRequested, this, nullptr);
        disconnect(_data_view, &MaskTableView::copyMasksRequested, this, nullptr);
        disconnect(_data_view, &MaskTableView::moveMasksToGroupRequested, this, nullptr);
        disconnect(_data_view, &MaskTableView::removeMasksFromGroupRequested, this, nullptr);
        disconnect(_data_view, &MaskTableView::deleteMasksRequested, this, nullptr);
    }

    _data_view = view;
    if (_data_view) {
        if (groupManager()) {
            _data_view->setGroupManager(groupManager());
        }
        
        // Connect to view signals for move/copy operations
        connect(_data_view, &MaskTableView::moveMasksRequested,
                this, &MaskInspector::_onMoveMasksRequested);
        connect(_data_view, &MaskTableView::copyMasksRequested,
                this, &MaskInspector::_onCopyMasksRequested);
        
        // Connect to view signals for group management operations
        connect(_data_view, &MaskTableView::moveMasksToGroupRequested,
                this, &MaskInspector::_onMoveMasksToGroupRequested);
        connect(_data_view, &MaskTableView::removeMasksFromGroupRequested,
                this, &MaskInspector::_onRemoveMasksFromGroupRequested);
        
        // Connect to view signal for delete operation
        connect(_data_view, &MaskTableView::deleteMasksRequested,
                this, &MaskInspector::_onDeleteMasksRequested);
    }
}

void MaskInspector::_populateGroupFilterCombo() {
    populateGroupFilterCombo(ui->groupFilterCombo, groupManager());
}

void MaskInspector::_onGroupFilterChanged(int index) {
    if (!_data_view || !groupManager()) {
        return;
    }

    if (index == 0) {
        // "All Groups" selected
        _data_view->clearGroupFilter();
    } else {
        // Specific group selected (index - 1 because index 0 is "All Groups")
        auto groups = groupManager()->getGroups();
        auto group_ids = groups.keys();
        if (index - 1 < group_ids.size()) {
            int group_id = group_ids[index - 1];
            _data_view->setGroupFilter(group_id);
        }
    }
}

void MaskInspector::_onGroupChanged() {
    // Store current selection and current text (in case index changes)
    int current_index = ui->groupFilterCombo->currentIndex();
    QString current_text;
    if (current_index >= 0 && current_index < ui->groupFilterCombo->count()) {
        current_text = ui->groupFilterCombo->itemText(current_index);
    }

    // Update the group filter combo box when groups change
    _populateGroupFilterCombo();

    // Restore selection
    restoreGroupFilterSelection(ui->groupFilterCombo, current_index, current_text);
}

void MaskInspector::_onMoveMasksRequested(std::string const & target_key) {
    if (!_data_view || _active_key.empty()) {
        return;
    }

    auto selected_entity_ids = _data_view->getSelectedEntityIds();
    if (selected_entity_ids.empty()) {
        std::cout << "MaskInspector: No masks selected to move." << std::endl;
        return;
    }

    std::cout << "MaskInspector: Moving " << selected_entity_ids.size()
              << " selected masks from '" << _active_key << "' to '" << target_key << "'..." << std::endl;

    std::size_t const total_moved = moveEntitiesByIds<MaskData>(
        dataManager().get(), _active_key, target_key, selected_entity_ids);

    if (total_moved > 0) {
        // Update the view to reflect changes
        _data_view->updateView();
        std::cout << "MaskInspector: Successfully moved " << total_moved
                  << " selected masks." << std::endl;
    } else {
        std::cout << "MaskInspector: No masks found with the selected EntityIds to move." << std::endl;
    }
}

void MaskInspector::_onCopyMasksRequested(std::string const & target_key) {
    if (!_data_view || _active_key.empty()) {
        return;
    }

    auto selected_entity_ids = _data_view->getSelectedEntityIds();
    if (selected_entity_ids.empty()) {
        std::cout << "MaskInspector: No masks selected to copy." << std::endl;
        return;
    }

    std::cout << "MaskInspector: Copying " << selected_entity_ids.size()
              << " selected masks from '" << _active_key << "' to '" << target_key << "'..." << std::endl;

    std::size_t const total_copied = copyEntitiesByIds<MaskData>(
        dataManager().get(), _active_key, target_key, selected_entity_ids);

    if (total_copied > 0) {
        std::cout << "MaskInspector: Successfully copied " << total_copied
                  << " selected masks." << std::endl;
    } else {
        std::cout << "MaskInspector: No masks found with the selected EntityIds to copy." << std::endl;
    }
}

void MaskInspector::_onMoveMasksToGroupRequested(int group_id) {
    if (!_data_view || !groupManager()) {
        return;
    }

    auto selected_entity_ids = _data_view->getSelectedEntityIds();
    if (selected_entity_ids.empty()) {
        std::cout << "MaskInspector: No masks selected to move to group." << std::endl;
        return;
    }

    std::unordered_set<EntityId> entity_ids_set(selected_entity_ids.begin(), selected_entity_ids.end());

    // First, remove entities from their current groups
    groupManager()->ungroupEntities(entity_ids_set);

    // Then, assign entities to the specified group
    groupManager()->assignEntitiesToGroup(group_id, entity_ids_set);

    // Refresh the view to show updated group information
    if (_data_view) {
        _data_view->updateView();
    }
}

void MaskInspector::_onRemoveMasksFromGroupRequested() {
    if (!_data_view || !groupManager()) {
        return;
    }

    auto selected_entity_ids = _data_view->getSelectedEntityIds();
    if (selected_entity_ids.empty()) {
        std::cout << "MaskInspector: No masks selected to remove from group." << std::endl;
        return;
    }

    std::unordered_set<EntityId> entity_ids_set(selected_entity_ids.begin(), selected_entity_ids.end());
    groupManager()->ungroupEntities(entity_ids_set);

    // Refresh the view to show updated group information
    if (_data_view) {
        _data_view->updateView();
    }
}

void MaskInspector::_onDeleteMasksRequested() {
    if (!_data_view || _active_key.empty()) {
        return;
    }

    auto selected_entity_ids = _data_view->getSelectedEntityIds();
    if (selected_entity_ids.empty()) {
        std::cout << "MaskInspector: No masks selected to delete." << std::endl;
        return;
    }

    auto mask_data = dataManager()->getData<MaskData>(_active_key);
    if (!mask_data) {
        std::cerr << "MaskInspector: MaskData object ('" << _active_key << "') not found." << std::endl;
        return;
    }

    std::cout << "MaskInspector: Deleting " << selected_entity_ids.size() << " selected masks..." << std::endl;

    // Remove entities from groups first
    if (groupManager()) {
        std::unordered_set<EntityId> entity_ids_set(selected_entity_ids.begin(), selected_entity_ids.end());
        groupManager()->ungroupEntities(entity_ids_set);
    }

    int total_masks_deleted = 0;

    // Delete each selected mask individually
    for (EntityId const entity_id : selected_entity_ids) {
        if (entity_id != EntityId(0)) {
            bool const success = mask_data->clearByEntityId(entity_id, NotifyObservers::No);
            if (success) {
                total_masks_deleted++;
            }
        }
    }

    // Notify observers only once at the end
    if (total_masks_deleted > 0) {
        mask_data->notifyObservers();

        // Update the view to reflect changes
        _data_view->updateView();

        std::cout << "MaskInspector: Successfully deleted " << total_masks_deleted
                  << " selected masks." << std::endl;
    } else {
        std::cout << "MaskInspector: No masks found to delete from the selected rows." << std::endl;
    }
}
