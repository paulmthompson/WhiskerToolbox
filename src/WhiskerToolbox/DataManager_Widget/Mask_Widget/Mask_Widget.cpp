#include "Mask_Widget.hpp"
#include "ui_Mask_Widget.h"

#include "CoreGeometry/masks.hpp"
#include "DataManager/DataManager.hpp"
#include "DataManager/Masks/Mask_Data.hpp"
#include "DataManager/Masks/utils/mask_utils.hpp"
#include "DataManager/Media/Media_Data.hpp"
#include "CoreGeometry/points.hpp"
#include "DataManager_Widget/utils/DataManager_Widget_utils.hpp"
#include "DataManager/IO/LoaderRegistry.hpp"
#include "DataManager/ConcreteDataFactory.hpp"
#include "MaskTableModel.hpp"

#include "utils/Deep_Learning/Models/EfficientSAM/EfficientSAM.hpp"

#include "IO_Widgets/Masks/HDF5/HDF5MaskSaver_Widget.hpp"
#include "IO_Widgets/Masks/Image/ImageMaskSaver_Widget.hpp"
#include "MediaExport/MediaExport_Widget.hpp"

#include <QCheckBox>
#include <QComboBox>
#include <QDir>
#include <QFileDialog>
#include <QMenu>
#include <QMessageBox>
#include <QPushButton>
#include <QStackedWidget>
#include <QString>
#include <QTableView>
#include <filesystem>
#include <iostream>
#include <set>

Mask_Widget::Mask_Widget(std::shared_ptr<DataManager> data_manager, QWidget * parent)
    : QWidget(parent),
      ui(new Ui::Mask_Widget),
      _data_manager{std::move(data_manager)} {
    ui->setupUi(this);

    _mask_table_model = new MaskTableModel(this);
    ui->tableView->setModel(_mask_table_model);
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

    _onExportTypeChanged(ui->export_type_combo->currentIndex());
    ui->media_export_options_widget->setVisible(ui->export_media_frames_checkbox->isChecked());
}

Mask_Widget::~Mask_Widget() {
    delete ui;
}

void Mask_Widget::openWidget() {
    this->show();
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
    int frame = _mask_table_model->getFrameForRow(index.row());
    if (frame != -1) {
        emit frameSelected(frame);
    }
}

std::vector<TimeFrameIndex> Mask_Widget::_getSelectedFrames() {
    std::vector<TimeFrameIndex> selected_frames;
    QModelIndexList selected_rows = ui->tableView->selectionModel()->selectedRows();

    for (QModelIndex const & index: selected_rows) {
        if (index.isValid()) {
            int frame = _mask_table_model->getFrameForRow(index.row());
            if (frame != -1) {
                selected_frames.push_back(TimeFrameIndex(frame));
            }
        }
    }

    return selected_frames;
}

void Mask_Widget::_showContextMenu(QPoint const & position) {
    QModelIndex index = ui->tableView->indexAt(position);
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

    // Add separator and delete operation
    context_menu.addSeparator();
    QAction * delete_action = context_menu.addAction("Delete Selected Masks");

    connect(delete_action, &QAction::triggered, this, &Mask_Widget::_deleteSelectedMasks);

    context_menu.exec(ui->tableView->mapToGlobal(position));
}

void Mask_Widget::_moveMasksToTarget(std::string const & target_key) {
    auto selected_frames = _getSelectedFrames();
    if (selected_frames.empty()) {
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

    std::cout << "Mask_Widget: Moving masks from " << selected_frames.size()
              << " frames from '" << _active_key << "' to '" << target_key << "'..." << std::endl;

    // Check if image sizes differ and we need to resize masks
    ImageSize source_size = source_mask_data->getImageSize();
    ImageSize target_size = target_mask_data->getImageSize();
    bool need_resize = (source_size.width != target_size.width || source_size.height != target_size.height);

    if (need_resize && (source_size.width <= 0 || source_size.height <= 0 ||
                        target_size.width <= 0 || target_size.height <= 0)) {
        std::cerr << "Mask_Widget: Cannot resize masks - invalid source (" << source_size.width
                  << "x" << source_size.height << ") or target (" << target_size.width
                  << "x" << target_size.height << ") image size." << std::endl;
        return;
    }

    std::size_t total_masks_moved = 0;

    if (need_resize) {
        std::cout << "Mask_Widget: Resizing masks from " << source_size.width << "x" << source_size.height
                  << " to " << target_size.width << "x" << target_size.height << std::endl;

        // Manual copy with resize, then delete from source
        for (auto frame: selected_frames) {
            auto const & masks_at_frame = source_mask_data->getAtTime(frame);
            if (!masks_at_frame.empty()) {
                for (auto const & mask: masks_at_frame) {
                    // Resize the mask to target dimensions
                    Mask2D resized_mask = resize_mask(mask, source_size, target_size);
                    if (!resized_mask.empty()) {
                        target_mask_data->addAtTime(frame, std::move(resized_mask), false);
                        total_masks_moved++;
                    }
                }
                // Clear the frame from source after copying all masks
                source_mask_data->clearAtTime(frame, false);
            }
        }

        // Notify observers for both source and target
        if (total_masks_moved > 0) {
            source_mask_data->notifyObservers();
            target_mask_data->notifyObservers();
        }
    } else {
        // Use the existing moveTo method when no resizing is needed
        total_masks_moved = source_mask_data->moveTo(*target_mask_data, selected_frames, true);
    }

    if (total_masks_moved > 0) {
        // Update the table view to reflect changes
        updateTable();

        std::cout << "Mask_Widget: Successfully moved " << total_masks_moved
                  << " masks from " << selected_frames.size() << " frames." << std::endl;
        if (need_resize) {
            std::cout << "Mask_Widget: Masks were resized from " << source_size.width << "x" << source_size.height
                      << " to " << target_size.width << "x" << target_size.height << std::endl;
        }
    } else {
        std::cout << "Mask_Widget: No masks found in any of the selected frames to move." << std::endl;
    }
}

void Mask_Widget::_copyMasksToTarget(std::string const & target_key) {
    auto selected_frames = _getSelectedFrames();
    if (selected_frames.empty()) {
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

    std::cout << "Mask_Widget: Copying masks from " << selected_frames.size()
              << " frames from '" << _active_key << "' to '" << target_key << "'..." << std::endl;

    // Check if image sizes differ and we need to resize masks
    ImageSize source_size = source_mask_data->getImageSize();
    ImageSize target_size = target_mask_data->getImageSize();
    bool need_resize = (source_size.width != target_size.width || source_size.height != target_size.height);

    if (need_resize && (source_size.width <= 0 || source_size.height <= 0 ||
                        target_size.width <= 0 || target_size.height <= 0)) {
        std::cerr << "Mask_Widget: Cannot resize masks - invalid source (" << source_size.width
                  << "x" << source_size.height << ") or target (" << target_size.width
                  << "x" << target_size.height << ") image size." << std::endl;
        return;
    }

    std::size_t total_masks_copied = 0;

    if (need_resize) {
        std::cout << "Mask_Widget: Resizing masks from " << source_size.width << "x" << source_size.height
                  << " to " << target_size.width << "x" << target_size.height << std::endl;

        // Manual copy with resize
        for (auto frame: selected_frames) {
            auto const & masks_at_frame = source_mask_data->getAtTime(frame);
            if (!masks_at_frame.empty()) {
                for (auto const & mask: masks_at_frame) {
                    // Resize the mask to target dimensions
                    Mask2D resized_mask = resize_mask(mask, source_size, target_size);
                    if (!resized_mask.empty()) {
                        target_mask_data->addAtTime(frame, std::move(resized_mask), false);
                        total_masks_copied++;
                    }
                }
            }
        }

        // Notify observers for target
        if (total_masks_copied > 0) {
            target_mask_data->notifyObservers();
        }
    } else {
        // Use the existing copyTo method when no resizing is needed
        total_masks_copied = source_mask_data->copyTo(*target_mask_data, selected_frames, true);
    }

    if (total_masks_copied > 0) {
        std::cout << "Mask_Widget: Successfully copied " << total_masks_copied
                  << " masks from " << selected_frames.size() << " frames." << std::endl;
        if (need_resize) {
            std::cout << "Mask_Widget: Masks were resized from " << source_size.width << "x" << source_size.height
                      << " to " << target_size.width << "x" << target_size.height << std::endl;
        }
    } else {
        std::cout << "Mask_Widget: No masks found in any of the selected frames to copy." << std::endl;
    }
}

void Mask_Widget::_deleteSelectedMasks() {
    auto selected_frames = _getSelectedFrames();
    if (selected_frames.empty()) {
        std::cout << "Mask_Widget: No masks selected to delete." << std::endl;
        return;
    }

    auto mask_data_ptr = _data_manager->getData<MaskData>(_active_key);
    if (!mask_data_ptr) {
        std::cerr << "Mask_Widget: Source MaskData object ('" << _active_key << "') not found for deletion." << std::endl;
        return;
    }

    std::cout << "Mask_Widget: Deleting masks from " << selected_frames.size()
              << " frames in '" << _active_key << "'..." << std::endl;

    int frames_with_masks = 0;
    int total_masks_deleted = 0;

    // Count masks before deletion and batch operations to minimize observer notifications
    for (auto frame: selected_frames) {
        auto masks_at_frame = mask_data_ptr->getAtTime(frame);
        if (!masks_at_frame.empty()) {
            frames_with_masks++;
            total_masks_deleted += static_cast<int>(masks_at_frame.size());
            mask_data_ptr->clearAtTime(frame, false);
        }
    }

    // Notify observers only once at the end
    if (frames_with_masks > 0) {
        mask_data_ptr->notifyObservers();

        // Update the table view to reflect changes
        updateTable();

        std::cout << "Mask_Widget: Successfully deleted " << total_masks_deleted
                  << " masks from " << frames_with_masks << " frames." << std::endl;
        if (frames_with_masks < selected_frames.size()) {
            std::cout << "Mask_Widget: Note: " << (selected_frames.size() - frames_with_masks)
                      << " selected frames contained no masks to delete." << std::endl;
        }
    } else {
        std::cout << "Mask_Widget: No masks found in any of the selected frames to delete." << std::endl;
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
            mask.push_back(Point2D<uint32_t>{static_cast<uint32_t>(i % image_size.width),
                                             static_cast<uint32_t>(i / image_size.width)});
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
    QString current_text = ui->export_type_combo->itemText(index);
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

void Mask_Widget::_initiateSaveProcess(QString const& format, MaskSaverConfig const& config) {
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
    std::string parent_dir = config.value("parent_dir", ".");
    updated_config["parent_dir"] = _data_manager->getOutputPath().string() + "/" + parent_dir;

    bool save_successful = _performRegistrySave(format, updated_config);

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
            // TODO: Update export_media_frames to work with JSON config
            QMessageBox::information(this, "Media Export", "Media frame export with JSON config not yet implemented.");
        }
    }
}

bool Mask_Widget::_performRegistrySave(QString const& format, MaskSaverConfig const& config) {
    auto mask_data_ptr = _data_manager->getData<MaskData>(_active_key);
    if (!mask_data_ptr) {
        QMessageBox::critical(this, "Save Error", "Critical: Could not retrieve MaskData for saving. Key: " + QString::fromStdString(_active_key));
        return false;
    }

    // Check if format is supported through registry
    LoaderRegistry& registry = LoaderRegistry::getInstance();
    std::string format_str = format.toStdString();
    
    if (!registry.isFormatSupported(format_str, IODataType::Mask)) {
        QMessageBox::warning(this, "Format Not Supported", 
            QString("Format '%1' saving is not available. This may require additional plugins to be enabled.\n\n"
                   "To enable format support:\n"
                   "1. Ensure required libraries are available in your build environment\n"
                   "2. Build with appropriate -DENABLE_* flags\n"
                   "3. Restart the application").arg(format));
        
        std::cout << "Format '" << format_str << "' saving not available - plugin not registered" << std::endl;
        return false;
    }

    try {
        // Use registry to save through the new save interface
        LoadResult result = registry.trySave(format_str, 
                                           IODataType::Mask, 
                                           "", // filepath not used for directory-based saving 
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
        ui->label_image_size_value->setText("Not Available");
        ui->label_image_size_value->setStyleSheet("color: rgb(100, 100, 100);");
        return;
    }

    auto mask_data = _data_manager->getData<MaskData>(_active_key);
    if (!mask_data) {
        ui->label_image_size_value->setText("Not Available");
        ui->label_image_size_value->setStyleSheet("color: rgb(100, 100, 100);");
        return;
    }

    ImageSize image_size = mask_data->getImageSize();

    if (image_size.width == -1 && image_size.height == -1) {
        ui->label_image_size_value->setText("Uninitialized (-1 x -1)");
        ui->label_image_size_value->setStyleSheet("color: rgb(255, 140, 0);");// Orange color for uninitialized
    } else {
        QString size_text = QString("%1 x %2").arg(image_size.height).arg(image_size.width);
        ui->label_image_size_value->setText(size_text);
        ui->label_image_size_value->setStyleSheet("color: rgb(0, 120, 0);");// Green color for valid size
    }
}
