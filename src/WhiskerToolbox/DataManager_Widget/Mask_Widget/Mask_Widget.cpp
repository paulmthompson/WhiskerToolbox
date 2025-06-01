#include "Mask_Widget.hpp"
#include "ui_Mask_Widget.h"

#include "DataManager/DataManager.hpp"
#include "DataManager/Masks/Mask_Data.hpp"
#include "DataManager/Media/Media_Data.hpp"
#include "DataManager/Points/points.hpp"
#include "DataManager_Widget/utils/DataManager_Widget_utils.hpp"
#include "MaskTableModel.hpp"

#include "utils/Deep_Learning/Models/EfficientSAM/EfficientSAM.hpp"

#include "IO_Widgets/Masks/HDF5/HDF5MaskSaver_Widget.hpp"
#include "IO_Widgets/Masks/Image/ImageMaskSaver_Widget.hpp"
#include "IO_Widgets/Media/MediaExport_Widget.hpp"

#include <QCheckBox>
#include <QComboBox>
#include <QDir>
#include <QFileDialog>
#include <QMessageBox>
#include <QPushButton>
#include <QStackedWidget>
#include <QTableView>
#include <filesystem>
#include <iostream>

Mask_Widget::Mask_Widget(std::shared_ptr<DataManager> data_manager, QWidget * parent)
    : QWidget(parent),
      ui(new Ui::Mask_Widget),
      _data_manager{std::move(data_manager)} {
    ui->setupUi(this);

    _mask_table_model = new MaskTableModel(this);
    ui->tableView->setModel(_mask_table_model);
    ui->tableView->setSelectionBehavior(QAbstractItemView::SelectRows);
    ui->tableView->setEditTriggers(QAbstractItemView::NoEditTriggers);

    connect(ui->load_sam_button, &QPushButton::clicked, this, &Mask_Widget::_loadSamModel);
    connect(ui->tableView, &QTableView::doubleClicked, this, &Mask_Widget::_handleTableViewDoubleClicked);
    connect(ui->moveMasksButton, &QPushButton::clicked, this, &Mask_Widget::_moveMasksButton_clicked);
    connect(ui->deleteMasksButton, &QPushButton::clicked, this, &Mask_Widget::_deleteMasksButton_clicked);

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
        _populateMoveToMaskDataComboBox();
        return;
    }
    removeCallbacks();

    _active_key = key;
    updateTable();
    _populateMoveToMaskDataComboBox();

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

void Mask_Widget::_populateMoveToMaskDataComboBox() {

    populate_move_combo_box<MaskData>(ui->moveToMaskDataComboBox, _data_manager.get(), _active_key);
}

void Mask_Widget::_moveMasksButton_clicked() {
    QModelIndexList selectedIndexes = ui->tableView->selectionModel()->selectedIndexes();
    if (selectedIndexes.isEmpty()) {
        std::cout << "Mask_Widget: No frame selected to move masks from." << std::endl;
        return;
    }
    int selected_row = selectedIndexes.first().row();
    int frame_to_move = _mask_table_model->getFrameForRow(selected_row);

    if (frame_to_move == -1) {
        std::cout << "Mask_Widget: Selected row data is invalid (no valid frame)." << std::endl;
        return;
    }

    QString target_key_qstr = ui->moveToMaskDataComboBox->currentText();
    if (target_key_qstr.isEmpty()) {
        std::cout << "Mask_Widget: No target MaskData selected in ComboBox." << std::endl;
        return;
    }
    std::string target_key = target_key_qstr.toStdString();

    auto source_mask_data = _data_manager->getData<MaskData>(_active_key);
    auto target_mask_data = _data_manager->getData<MaskData>(target_key);

    if (!source_mask_data) {
        std::cerr << "Mask_Widget: Source MaskData ('" << _active_key << "') not found." << std::endl;
        return;
    }
    if (!target_mask_data) {
        std::cerr << "Mask_Widget: Target MaskData ('" << target_key << "') not found." << std::endl;
        return;
    }

    std::vector<Mask2D> const & masks_to_move = source_mask_data->getAtTime(frame_to_move);
    if (!masks_to_move.empty()) {
        for (Mask2D const & individual_mask: masks_to_move) {
            target_mask_data->addAtTime(frame_to_move, individual_mask);// Add one by one
        }
    }

    source_mask_data->clearAtTime(frame_to_move);// Clear all masks from source at this frame

    updateTable();
    _populateMoveToMaskDataComboBox();

    std::cout << "Masks from frame " << frame_to_move << " moved from " << _active_key << " to " << target_key << std::endl;
}

void Mask_Widget::_deleteMasksButton_clicked() {
    QModelIndexList selectedIndexes = ui->tableView->selectionModel()->selectedIndexes();
    if (selectedIndexes.isEmpty()) {
        std::cout << "Mask_Widget: No frame selected to delete masks from." << std::endl;
        return;
    }
    int selected_row = selectedIndexes.first().row();
    int frame_to_delete = _mask_table_model->getFrameForRow(selected_row);

    if (frame_to_delete == -1) {
        std::cout << "Mask_Widget: Selected row data for deletion is invalid (no valid frame)." << std::endl;
        return;
    }

    auto source_mask_data = _data_manager->getData<MaskData>(_active_key);
    if (!source_mask_data) {
        std::cerr << "Mask_Widget: Source MaskData ('" << _active_key << "') not found for deletion." << std::endl;
        return;
    }

    source_mask_data->clearAtTime(frame_to_delete);

    updateTable();
    _populateMoveToMaskDataComboBox();

    std::cout << "Masks deleted from frame " << frame_to_delete << " in " << _active_key << std::endl;
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

    auto processed_frame_data = media->getProcessedData(current_time);
    if (processed_frame_data.empty()) {
        std::cerr << "Mask_Widget: Processed frame data is empty." << std::endl;
        return;
    }

    auto mask_image = _sam_model->process_frame(
            processed_frame_data,
            image_size,
            static_cast<int>(std::round(x)),
            static_cast<int>(std::round(y)));

    std::vector<Point2D<float>> mask;
    for (size_t i = 0; i < mask_image.size(); i++) {
        if (mask_image[i] > 0) {
            mask.push_back(Point2D<float>{static_cast<float>(i % image_size.width),
                                          static_cast<float>(i / image_size.width)});
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

    active_mask_data->addAtTime(current_time, mask);
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

void Mask_Widget::_handleSaveImageMaskRequested(ImageMaskSaverOptions options) {
    MaskSaverOptionsVariant options_variant = options;
    _initiateSaveProcess(SaverType::IMAGE, options_variant);
}

void Mask_Widget::_onExportMediaFramesCheckboxToggled(bool checked) {
    ui->media_export_options_widget->setVisible(checked);
}

void Mask_Widget::_initiateSaveProcess(SaverType saver_type, MaskSaverOptionsVariant & options_variant) {
    if (_active_key.empty()) {
        QMessageBox::warning(this, "No Data Selected", "Please select a MaskData item to save.");
        return;
    }

    auto mask_data_ptr = _data_manager->getData<MaskData>(_active_key);
    if (!mask_data_ptr) {
        QMessageBox::critical(this, "Error", "Could not retrieve MaskData for saving. Key: " + QString::fromStdString(_active_key));
        return;
    }

    bool save_successful = false;
    std::string saved_parent_dir;

    switch (saver_type) {
        case SaverType::IMAGE: {
            ImageMaskSaverOptions & specific_image_options = std::get<ImageMaskSaverOptions>(options_variant);
            specific_image_options.parent_dir = _data_manager->getOutputPath().string() + "/" + specific_image_options.parent_dir;
            saved_parent_dir = specific_image_options.parent_dir;
            save_successful = _performActualImageSave(specific_image_options);
            break;
        }
        case SaverType::HDF5: {
            // TODO: Implement HDF5 saving when HDF5MaskSaver_Widget is complete
            QMessageBox::information(this, "Not Implemented", "HDF5 mask saving is not yet implemented.");
            return;
        }
    }

    if (!save_successful) {
        return;
    }

    if (ui->export_media_frames_checkbox->isChecked()) {
        auto times_with_data = mask_data_ptr->getTimesWithData();
        std::vector<size_t> frame_ids_to_export = times_with_data;

        if (frame_ids_to_export.empty()) {
            QMessageBox::information(this, "No Frames", "No masks found in data, so no media frames to export.");
        } else {
            export_media_frames(_data_manager.get(),
                                ui->media_export_options_widget,
                                options_variant,
                                this,
                                frame_ids_to_export);
        }
    }
}

bool Mask_Widget::_performActualImageSave(ImageMaskSaverOptions & options) {
    auto mask_data_ptr = _data_manager->getData<MaskData>(_active_key);
    if (!mask_data_ptr) {
        QMessageBox::critical(this, "Save Error", "Critical: Could not retrieve MaskData for saving. Key: " + QString::fromStdString(_active_key));
        return false;
    }

    try {
        save(mask_data_ptr.get(), options);
        QMessageBox::information(this, "Save Successful", QString::fromStdString("Mask data saved to directory: " + options.parent_dir));
        std::cout << "Mask data saved to directory: " << options.parent_dir << std::endl;
        return true;
    } catch (std::exception const & e) {
        QMessageBox::critical(this, "Save Error", "Failed to save mask data: " + QString::fromStdString(e.what()));
        std::cerr << "Failed to save mask data: " << e.what() << std::endl;
        return false;
    }
}
