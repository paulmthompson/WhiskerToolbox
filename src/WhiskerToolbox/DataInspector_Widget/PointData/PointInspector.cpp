#include "PointInspector.hpp"
#include "ui_PointInspector.h"

#include "PointTableView.hpp"

#include "DataManager.hpp"
#include "DataManager/Media/Media_Data.hpp"
#include "DataManager/Points/Point_Data.hpp"
#include "DataExport_Widget/Points/CSV/CSVPointSaver_Widget.hpp"
#include "MediaExport/MediaExport_Widget.hpp"
#include "MediaExport/media_export.hpp"
#include "GroupManagementWidget/GroupManager.hpp"

#include "CoreGeometry/ImageSize.hpp"

#include <QCheckBox>
#include <QComboBox>
#include <QLineEdit>
#include <QMessageBox>
#include <QPushButton>
#include <QStackedWidget>

#include <filesystem>
#include <iostream>

PointInspector::PointInspector(
    std::shared_ptr<DataManager> data_manager,
    GroupManager * group_manager,
    QWidget * parent)
    : BaseInspector(std::move(data_manager), group_manager, parent)
    , ui(new Ui::PointInspector) {
    ui->setupUi(this);

    connect(ui->export_type_combo, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &PointInspector::_onExportTypeChanged);

    connect(ui->csv_point_saver_widget, &CSVPointSaver_Widget::saveCSVRequested,
            this, &PointInspector::_handleSaveCSVRequested);

    connect(ui->export_media_frames_checkbox, &QCheckBox::toggled,
            this, &PointInspector::_onExportMediaFramesCheckboxToggled);
    connect(ui->apply_image_size_button, &QPushButton::clicked,
            this, &PointInspector::_onApplyImageSizeClicked);
    connect(ui->copy_image_size_button, &QPushButton::clicked,
            this, &PointInspector::_onCopyImageSizeClicked);
    connect(ui->groupFilterCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &PointInspector::_onGroupFilterChanged);

    // Setup collapsible export section
    ui->export_section->autoSetContentLayout();
    ui->export_section->setTitle("Export Options");
    ui->export_section->toggle(false);// Start collapsed

    _onExportTypeChanged(ui->export_type_combo->currentIndex());
    ui->media_export_options_widget->setVisible(ui->export_media_frames_checkbox->isChecked());

    // Populate media combo box
    _populateMediaComboBox();

    // Set up callback to refresh media combo box when data changes
    if (dataManager()) {
        _dm_observer_id = dataManager()->addObserver([this]() {
            _populateMediaComboBox();
        });
    }

    // Set up group manager (will connect signals and populate combo)
    if (groupManager()) {
        setGroupManager(groupManager());
    }
}

PointInspector::~PointInspector() {
    removeCallbacks();
    // Remove DataManager-level observer
    if (dataManager() && _dm_observer_id != -1) {
        dataManager()->removeObserver(_dm_observer_id);
    }
    delete ui;
}

void PointInspector::setActiveKey(std::string const & key) {
    if (_active_key == key && dataManager()->getData<PointData>(key)) {
        return;
    }
    removeCallbacks();
    _active_key = key;
    auto point_data = dataManager()->getData<PointData>(_active_key);
    if (point_data) {
        _updateImageSizeDisplay();
    }
}

void PointInspector::removeCallbacks() {
    // No data-level callbacks needed here since table is in PointTableView
}

void PointInspector::updateView() {
    // Update image size display when data changes
    if (!_active_key.empty()) {
        _updateImageSizeDisplay();
    }
}

void PointInspector::setTableView(PointTableView * table_view) {
    _table_view = table_view;
    if (_table_view && groupManager()) {
        _table_view->setGroupManager(groupManager());
    }
}

void PointInspector::setGroupManager(GroupManager * group_manager) {
    BaseInspector::setGroupManager(group_manager);
    if (_table_view) {
        _table_view->setGroupManager(group_manager);
    }
    // Disconnect old connections if any
    if (groupManager()) {
        disconnect(groupManager(), nullptr, this, nullptr);
    }
    if (group_manager) {
        connect(group_manager, &GroupManager::groupCreated,
                this, &PointInspector::_onGroupChanged);
        connect(group_manager, &GroupManager::groupRemoved,
                this, &PointInspector::_onGroupChanged);
        connect(group_manager, &GroupManager::groupModified,
                this, &PointInspector::_onGroupChanged);
        _populateGroupFilterCombo();
    }
}

void PointInspector::_onExportTypeChanged(int index) {
    QString const current_text = ui->export_type_combo->itemText(index);
    if (current_text == "CSV") {
        ui->stacked_saver_options->setCurrentWidget(ui->csv_point_saver_widget);
    }
}

void PointInspector::_handleSaveCSVRequested(CSVPointSaverOptions csv_options) {
    PointSaverOptionsVariant options_variant = csv_options;
    _initiateSaveProcess(SaverType::CSV, options_variant);
}

void PointInspector::_initiateSaveProcess(SaverType saver_type, PointSaverOptionsVariant & options_variant) {
    if (_active_key.empty() || !dataManager()->getData<PointData>(_active_key)) {
        QMessageBox::warning(this, "No Data", "No active point data to save.");
        return;
    }

    auto point_data_ptr = dataManager()->getData<PointData>(_active_key);
    if (!point_data_ptr) {
        QMessageBox::critical(this, "Error", "Could not retrieve data for saving.");
        return;
    }

    bool save_successful = false;
    switch (saver_type) {
        case SaverType::CSV: {
            auto & specific_csv_options = std::get<CSVPointSaverOptions>(options_variant);
            specific_csv_options.parent_dir = dataManager()->getOutputPath();
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
            auto media_ptr = dataManager()->getData<MediaData>("media");
            if (!media_ptr) {
                QMessageBox::warning(this, "Media Not Available", "Could not access media for exporting frames.");
            } else {
                // Derive output dir from point save options (CSV in this branch)
                std::string base_output_dir;
                if (std::holds_alternative<CSVPointSaverOptions>(options_variant)) {
                    base_output_dir = std::get<CSVPointSaverOptions>(options_variant).parent_dir;
                } else {
                    base_output_dir = dataManager()->getOutputPath();
                }

                MediaExportOptions options = ui->media_export_options_widget->getOptions();
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
        } else {
            QMessageBox::information(this, "No Frames", "No points found in data, so no media frames to export.");
        }
    }
}

bool PointInspector::_performActualCSVSave(CSVPointSaverOptions & options) {
    auto point_data_ptr = dataManager()->getData<PointData>(_active_key);
    if (!point_data_ptr) {
        std::cerr << "_performActualCSVSave: Critical - Could not get PointData for key: " << _active_key << std::endl;
        return false;
    }

    save(point_data_ptr.get(), options);
    QMessageBox::information(this, "Save Successful", QString::fromStdString("Points data saved to " + options.parent_dir + "/" + options.filename));
    std::cout << "Point data saved to: " << options.parent_dir << "/" << options.filename << std::endl;
    return true;
}

void PointInspector::_onExportMediaFramesCheckboxToggled(bool checked) {
    ui->media_export_options_widget->setVisible(checked);
}

void PointInspector::_onApplyImageSizeClicked() {
    if (_active_key.empty()) {
        QMessageBox::warning(this, "No Data Selected", "Please select a PointData item to modify image size.");
        return;
    }

    auto point_data = dataManager()->getData<PointData>(_active_key);
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
                                         .arg(new_width)
                                         .arg(new_height));
        return;
    }

    // Ask user if they want to scale existing data
    int ret = QMessageBox::question(this, "Scale Existing Data",
                                    QString("Current image size is %1 × %2. Do you want to scale all existing point data to the new size %3 × %4?\n\n"
                                            "Click 'Yes' to scale all point data proportionally.\n"
                                            "Click 'No' to just change the image size without scaling.\n"
                                            "Click 'Cancel' to abort the operation.")
                                            .arg(current_size.width)
                                            .arg(current_size.height)
                                            .arg(new_width)
                                            .arg(new_height),
                                    QMessageBox::Yes | QMessageBox::No | QMessageBox::Cancel);

    if (ret == QMessageBox::Cancel) {
        return;
    }

    if (ret == QMessageBox::Yes) {
        // Scale the data
        point_data->changeImageSize({new_width, new_height});
        QMessageBox::information(this, "Image Size Changed",
                                 QString("Image size changed to %1 × %2 and all point data has been scaled proportionally.")
                                         .arg(new_width)
                                         .arg(new_height));
    } else {
        // Just set the new size without scaling
        point_data->setImageSize({new_width, new_height});
        QMessageBox::information(this, "Image Size Set",
                                 QString("Image size set to %1 × %2 (existing point data was not scaled).")
                                         .arg(new_width)
                                         .arg(new_height));
    }

    _updateImageSizeDisplay();
}

void PointInspector::_updateImageSizeDisplay() {
    if (_active_key.empty()) {
        ui->image_width_edit->setText("");
        ui->image_height_edit->setText("");
        ui->image_size_status_label->setText("No Data Selected");
        ui->image_size_status_label->setStyleSheet("color: #666666; font-style: italic;");
        std::cout << "PointInspector::_updateImageSizeDisplay: No active key" << std::endl;
        return;
    }

    auto point_data = dataManager()->getData<PointData>(_active_key);
    if (!point_data) {
        ui->image_width_edit->setText("");
        ui->image_height_edit->setText("");
        ui->image_size_status_label->setText("Data Not Found");
        ui->image_size_status_label->setStyleSheet("color: #cc0000; font-style: italic;");
        std::cout << "PointInspector::_updateImageSizeDisplay: No point data found for key: " << _active_key << std::endl;
        return;
    }

    ImageSize current_size = point_data->getImageSize();
    std::cout << "PointInspector::_updateImageSizeDisplay: Current size: " << current_size.width << " x " << current_size.height << std::endl;

    if (current_size.width == -1 || current_size.height == -1) {
        ui->image_width_edit->setText("");
        ui->image_height_edit->setText("");
        ui->image_size_status_label->setText("Not Set");
        ui->image_size_status_label->setStyleSheet("color: #666666; font-style: italic;");
        std::cout << "PointInspector::_updateImageSizeDisplay: No size set, clearing fields" << std::endl;
    } else {
        ui->image_width_edit->setText(QString::number(current_size.width));
        ui->image_height_edit->setText(QString::number(current_size.height));
        ui->image_size_status_label->setText(QString("%1 × %2").arg(current_size.width).arg(current_size.height));
        ui->image_size_status_label->setStyleSheet("color: #000000; font-weight: bold;");
        std::cout << "PointInspector::_updateImageSizeDisplay: Set fields to " << current_size.width << " x " << current_size.height << std::endl;
    }
}

void PointInspector::_onCopyImageSizeClicked() {
    if (_active_key.empty()) {
        QMessageBox::warning(this, "No Data Selected", "Please select a PointData item to modify image size.");
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

    auto point_data = dataManager()->getData<PointData>(_active_key);
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
                                         .arg(media_size.width)
                                         .arg(media_size.height)
                                         .arg(selected_media_key));
        return;
    }

    // Ask user if they want to scale existing data
    int ret = QMessageBox::question(this, "Scale Existing Data",
                                    QString("Current image size is %1 × %2. Do you want to scale all existing point data to the new size %3 × %4 (from '%5')?\n\n"
                                            "Click 'Yes' to scale all point data proportionally.\n"
                                            "Click 'No' to just change the image size without scaling.\n"
                                            "Click 'Cancel' to abort the operation.")
                                            .arg(current_size.width)
                                            .arg(current_size.height)
                                            .arg(media_size.width)
                                            .arg(media_size.height)
                                            .arg(selected_media_key),
                                    QMessageBox::Yes | QMessageBox::No | QMessageBox::Cancel);

    if (ret == QMessageBox::Cancel) {
        return;
    }

    if (ret == QMessageBox::Yes) {
        // Scale the data
        point_data->changeImageSize(media_size);
        QMessageBox::information(this, "Image Size Changed",
                                 QString("Image size changed to %1 × %2 (copied from '%3') and all point data has been scaled proportionally.")
                                         .arg(media_size.width)
                                         .arg(media_size.height)
                                         .arg(selected_media_key));
    } else {
        // Just set the new size without scaling
        point_data->setImageSize(media_size);
        QMessageBox::information(this, "Image Size Set",
                                 QString("Image size set to %1 × %2 (copied from '%3', existing point data was not scaled).")
                                         .arg(media_size.width)
                                         .arg(media_size.height)
                                         .arg(selected_media_key));
    }

    _updateImageSizeDisplay();
}

void PointInspector::_populateMediaComboBox() {
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

    for (auto const & key: media_keys) {
        ui->copy_from_media_combo->addItem(QString::fromStdString(key));
    }

    std::cout << "PointInspector::_populateMediaComboBox: Found " << media_keys.size() << " media keys" << std::endl;
}

void PointInspector::_onGroupFilterChanged(int index) {
    if (!groupManager() || !_table_view) {
        return;
    }

    if (index == 0) {
        // "All Groups" selected
        _table_view->clearGroupFilter();
    } else {
        // Specific group selected (index - 1 because index 0 is "All Groups")
        auto groups = groupManager()->getGroups();
        auto group_ids = groups.keys();
        if (index - 1 < group_ids.size()) {
            int group_id = group_ids[index - 1];
            _table_view->setGroupFilter(group_id);
        }
    }
}

void PointInspector::_onGroupChanged() {
    // Store current selection
    int current_index = ui->groupFilterCombo->currentIndex();

    // Update the group filter combo box when groups change
    _populateGroupFilterCombo();

    // If the previously selected group no longer exists, reset to "All Groups"
    if (current_index > 0 && current_index >= ui->groupFilterCombo->count()) {
        ui->groupFilterCombo->setCurrentIndex(0);// "All Groups"
        if (_table_view) {
            _table_view->clearGroupFilter();
        }
    }
}

void PointInspector::_populateGroupFilterCombo() {
    ui->groupFilterCombo->clear();
    ui->groupFilterCombo->addItem("All Groups");

    if (!groupManager()) {
        return;
    }

    auto groups = groupManager()->getGroups();
    for (auto it = groups.begin(); it != groups.end(); ++it) {
        ui->groupFilterCombo->addItem(it.value().name);
    }
}
