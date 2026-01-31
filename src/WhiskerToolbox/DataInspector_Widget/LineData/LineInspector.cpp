#include "LineInspector.hpp"
#include "ui_LineInspector.h"

#include "LineTableView.hpp"
#include "DataInspector_Widget/Inspectors/GroupFilterHelper.hpp"
#include "DataInspector_Widget/DataInspectorState.hpp"

#include "DataManager.hpp"
#include "DataManager/IO/core/LoaderRegistry.hpp"
#include "DataManager/IO/core/IOTypes.hpp"
#include "DataManager/Lines/Line_Data.hpp"
#include "DataManager/Media/Media_Data.hpp"

#include "DataManager_Widget/utils/DataManager_Widget_utils.hpp"
#include "DataExport_Widget/Lines/Binary/BinaryLineSaver_Widget.hpp"
#include "DataExport_Widget/Lines/CSV/CSVLineSaver_Widget.hpp"
#include "MediaExport/MediaExport_Widget.hpp"
#include "WhiskerToolbox/GroupManagementWidget/GroupManager.hpp"
#include "MediaExport/media_export.hpp"

#include "CoreGeometry/ImageSize.hpp"

#include <QCheckBox>
#include <QComboBox>
#include <QDir>
#include <QFileDialog>
#include <QLineEdit>
#include <QMessageBox>
#include <QPushButton>
#include <QStackedWidget>
#include <QVBoxLayout>

#include <filesystem>
#include <iostream>
#include <set>
#include <unordered_set>
#include <vector>

LineInspector::LineInspector(
    std::shared_ptr<DataManager> data_manager,
    GroupManager * group_manager,
    QWidget * parent)
    : BaseInspector(std::move(data_manager), group_manager, parent)
    , _ui(new Ui::LineInspector) {
    _ui->setupUi(this);
    _setupUi();
    _connectSignals();

    // Setup collapsible export section
    _ui->export_section->autoSetContentLayout();
    _ui->export_section->setTitle("Export Options");
    _ui->export_section->toggle(false);  // Start collapsed

    _onExportTypeChanged(_ui->export_type_combo->currentIndex());
    _ui->media_export_options_widget->setVisible(_ui->export_media_frames_checkbox->isChecked());

    // Populate media combo box
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

LineInspector::~LineInspector() {
    removeCallbacks();
    // Remove DataManager-level observer
    if (dataManager() && _dm_observer_id != -1) {
        dataManager()->removeObserver(_dm_observer_id);
    }
    delete _ui;
}

void LineInspector::setActiveKey(std::string const & key) {
    if (_active_key == key) {
        return;
    }

    removeCallbacks();
    _active_key = key;

    _updateImageSizeDisplay();
}

void LineInspector::removeCallbacks() {
    // No callbacks to remove - the table view handles its own callbacks
}

void LineInspector::updateView() {
    _updateImageSizeDisplay();
}

void LineInspector::_setupUi() {
    // UI is already set up by ui->setupUi(this) in constructor
    // No additional setup needed
}

void LineInspector::_connectSignals() {
    // Export signals
    connect(_ui->export_type_combo, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &LineInspector::_onExportTypeChanged);
    connect(_ui->csv_line_saver_widget, &CSVLineSaver_Widget::saveCSVRequested,
            this, &LineInspector::_handleSaveCSVRequested);
    connect(_ui->csv_line_saver_widget, &CSVLineSaver_Widget::saveMultiFileCSVRequested,
            this, &LineInspector::_handleSaveMultiFileCSVRequested);
    connect(_ui->binary_line_saver_widget, &BinaryLineSaver_Widget::saveBinaryRequested,
            this, &LineInspector::_handleSaveBinaryRequested);
    connect(_ui->export_media_frames_checkbox, &QCheckBox::toggled,
            this, &LineInspector::_onExportMediaFramesCheckboxToggled);

    // Image size signals
    connect(_ui->apply_image_size_button, &QPushButton::clicked,
            this, &LineInspector::_onApplyImageSizeClicked);
    connect(_ui->copy_image_size_button, &QPushButton::clicked,
            this, &LineInspector::_onCopyImageSizeClicked);

    // Group filter signals
    connect(_ui->groupFilterCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &LineInspector::_onGroupFilterChanged);
    connect(_ui->autoScrollButton, &QPushButton::clicked,
            this, &LineInspector::_onAutoScrollToCurrentFrame);

    // Group manager signals
    connectGroupManagerSignals(groupManager(), this, &LineInspector::_onGroupChanged);
}

void LineInspector::_onExportTypeChanged(int index) {
    QString const current_text = _ui->export_type_combo->itemText(index);
    if (current_text == "CSV") {
        _ui->stacked_saver_options->setCurrentWidget(_ui->csv_line_saver_widget);
    } else if (current_text == "Binary") {
        _ui->stacked_saver_options->setCurrentWidget(_ui->binary_line_saver_widget);
    }
}

void LineInspector::_handleSaveCSVRequested(QString const & format, nlohmann::json const & config) {
    _initiateSaveProcess(format, config);
}

void LineInspector::_handleSaveMultiFileCSVRequested(QString const & format, nlohmann::json const & config) {
    _initiateSaveProcess(format, config);
}

void LineInspector::_handleSaveBinaryRequested(QString const & format, nlohmann::json const & config) {
    _initiateSaveProcess(format, config);
}

void LineInspector::_onExportMediaFramesCheckboxToggled(bool checked) {
    _ui->media_export_options_widget->setVisible(checked);
}

void LineInspector::_initiateSaveProcess(QString const & format, LineSaverConfig const & config) {
    if (_active_key.empty()) {
        QMessageBox::warning(this, "No Data Selected", "Please select a LineData item to save.");
        return;
    }

    auto line_data_ptr = dataManager()->getData<LineData>(_active_key);
    if (!line_data_ptr) {
        QMessageBox::critical(this, "Error", "Could not retrieve LineData for saving. Key: " + QString::fromStdString(_active_key));
        return;
    }

    // Update config with full path
    LineSaverConfig updated_config = config;
    std::string parent_dir = config.value("parent_dir", ".");

    // Only prepend output path if parent_dir is relative (starts with "." or doesn't start with "/")
    if (parent_dir == "." || (!parent_dir.empty() && parent_dir[0] != '/')) {
        if (parent_dir == ".") {
            updated_config["parent_dir"] = dataManager()->getOutputPath();
        } else {
            updated_config["parent_dir"] = dataManager()->getOutputPath() + "/" + parent_dir;
        }
    }
    // If parent_dir is absolute, use it as-is

    bool const save_successful = _performRegistrySave(format, updated_config);

    if (!save_successful) {
        return;
    }

    if (_ui->export_media_frames_checkbox->isChecked()) {
        auto times_with_data = line_data_ptr->getTimesWithData();
        std::vector<size_t> frame_ids_to_export;
        frame_ids_to_export.reserve(times_with_data.size());
        for (auto frame_id: times_with_data) {
            frame_ids_to_export.push_back(static_cast<size_t>(frame_id.getValue()));
        }

        if (frame_ids_to_export.empty()) {
            QMessageBox::information(this, "No Frames", "No lines found in data, so no media frames to export.");
        } else {
            auto media_ptr = dataManager()->getData<MediaData>("media");
            if (!media_ptr) {
                QMessageBox::warning(this, "Media Not Available", "Could not access media for exporting frames.");
                return;
            }

            MediaExportOptions options = _ui->media_export_options_widget->getOptions();
            // Use the same parent directory used for line saving
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

bool LineInspector::_performRegistrySave(QString const & format, LineSaverConfig const & config) {
    auto line_data_ptr = dataManager()->getData<LineData>(_active_key);
    if (!line_data_ptr) {
        QMessageBox::critical(this, "Save Error", "Critical: Could not retrieve LineData for saving. Key: " + QString::fromStdString(_active_key));
        return false;
    }

    // Check if format is supported through registry
    LoaderRegistry & registry = LoaderRegistry::getInstance();
    std::string const format_str = format.toStdString();

    if (!registry.isFormatSupported(format_str, IODataType::Line)) {
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
        // Construct filepath for the registry (though CSVLoader doesn't use it directly)
        std::string const save_type = config.value("save_type", "single");
        std::string filepath;

        if (save_type == "single") {
            std::string const parent_dir = config.value("parent_dir", ".");
            std::string const filename = config.value("filename", "line_data.csv");
            filepath = parent_dir + "/" + filename;
        } else if (save_type == "multi") {
            filepath = config.value("parent_dir", ".");
        }

        // Use registry to save through the new save interface
        LoadResult const result = registry.trySave(format_str,
                                                   IODataType::Line,
                                                   filepath,
                                                   config,
                                                   line_data_ptr.get());

        if (result.success) {
            std::string const save_location = config.value("parent_dir", ".");
            QMessageBox::information(this, "Save Successful",
                                     QString("Line data saved successfully to: %1").arg(QString::fromStdString(save_location)));
            std::cout << "Line data saved successfully using " << format_str << " format" << std::endl;
            return true;
        } else {
            QMessageBox::critical(this, "Save Error",
                                  QString("Failed to save line data: %1").arg(QString::fromStdString(result.error_message)));
            std::cerr << "Failed to save line data: " << result.error_message << std::endl;
            return false;
        }

    } catch (std::exception const & e) {
        QMessageBox::critical(this, "Save Error", "Failed to save line data: " + QString::fromStdString(e.what()));
        std::cerr << "Failed to save line data: " << e.what() << std::endl;
        return false;
    }
}

void LineInspector::_onApplyImageSizeClicked() {
    if (_active_key.empty()) {
        QMessageBox::warning(this, "No Data Selected", "Please select a LineData item to modify image size.");
        return;
    }

    auto line_data = dataManager()->getData<LineData>(_active_key);
    if (!line_data) {
        QMessageBox::critical(this, "Error", "Could not retrieve LineData for image size modification. Key: " + QString::fromStdString(_active_key));
        return;
    }

    // Get current values from the line edits
    QString width_text = _ui->image_width_edit->text().trimmed();
    QString height_text = _ui->image_height_edit->text().trimmed();

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
    ImageSize current_size = line_data->getImageSize();

    // If no current size is set, just set the new size without scaling
    if (current_size.width == -1 || current_size.height == -1) {
        line_data->setImageSize({new_width, new_height});
        _updateImageSizeDisplay();
        QMessageBox::information(this, "Image Size Set",
                                 QString("Image size set to %1 × %2 (no scaling applied as no previous size was set).")
                                         .arg(new_width)
                                         .arg(new_height));
        return;
    }

    // Ask user if they want to scale existing data
    int ret = QMessageBox::question(this, "Scale Existing Data",
                                    QString("Current image size is %1 × %2. Do you want to scale all existing line data to the new size %3 × %4?\n\n"
                                            "Click 'Yes' to scale all line data proportionally.\n"
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
        line_data->changeImageSize({new_width, new_height});
        QMessageBox::information(this, "Image Size Changed",
                                 QString("Image size changed to %1 × %2 and all line data has been scaled proportionally.")
                                         .arg(new_width)
                                         .arg(new_height));
    } else {
        // Just set the new size without scaling
        line_data->setImageSize({new_width, new_height});
        QMessageBox::information(this, "Image Size Set",
                                 QString("Image size set to %1 × %2 (existing line data was not scaled).")
                                         .arg(new_width)
                                         .arg(new_height));
    }

    _updateImageSizeDisplay();
}

void LineInspector::_updateImageSizeDisplay() {
    if (_active_key.empty()) {
        _ui->image_width_edit->setText("");
        _ui->image_height_edit->setText("");
        _ui->image_size_status_label->setText("No Data Selected");
        _ui->image_size_status_label->setStyleSheet("color: #666666; font-style: italic;");
        return;
    }

    auto line_data = dataManager()->getData<LineData>(_active_key);
    if (!line_data) {
        _ui->image_width_edit->setText("");
        _ui->image_height_edit->setText("");
        _ui->image_size_status_label->setText("Data Not Found");
        _ui->image_size_status_label->setStyleSheet("color: #cc0000; font-style: italic;");
        return;
    }

    ImageSize current_size = line_data->getImageSize();

    if (current_size.width == -1 || current_size.height == -1) {
        _ui->image_width_edit->setText("");
        _ui->image_height_edit->setText("");
        _ui->image_size_status_label->setText("Not Set");
        _ui->image_size_status_label->setStyleSheet("color: #666666; font-style: italic;");
    } else {
        _ui->image_width_edit->setText(QString::number(current_size.width));
        _ui->image_height_edit->setText(QString::number(current_size.height));
        _ui->image_size_status_label->setText(QString("%1 × %2").arg(current_size.width).arg(current_size.height));
        _ui->image_size_status_label->setStyleSheet("color: #000000; font-weight: bold;");
    }
}

void LineInspector::_onCopyImageSizeClicked() {
    if (_active_key.empty()) {
        QMessageBox::warning(this, "No Data Selected", "Please select a LineData item to modify image size.");
        return;
    }

    QString selected_media_key = _ui->copy_from_media_combo->currentText();
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

    auto line_data = dataManager()->getData<LineData>(_active_key);
    if (!line_data) {
        QMessageBox::critical(this, "Error", "Could not retrieve LineData for image size modification. Key: " + QString::fromStdString(_active_key));
        return;
    }

    // Get current image size
    ImageSize current_size = line_data->getImageSize();

    // If no current size is set, just set the new size without scaling
    if (current_size.width == -1 || current_size.height == -1) {
        line_data->setImageSize(media_size);
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
                                    QString("Current image size is %1 × %2. Do you want to scale all existing line data to the new size %3 × %4 (from '%5')?\n\n"
                                            "Click 'Yes' to scale all line data proportionally.\n"
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
        line_data->changeImageSize(media_size);
        QMessageBox::information(this, "Image Size Changed",
                                 QString("Image size changed to %1 × %2 (copied from '%3') and all line data has been scaled proportionally.")
                                         .arg(media_size.width)
                                         .arg(media_size.height)
                                         .arg(selected_media_key));
    } else {
        // Just set the new size without scaling
        line_data->setImageSize(media_size);
        QMessageBox::information(this, "Image Size Set",
                                 QString("Image size set to %1 × %2 (copied from '%3', existing line data was not scaled).")
                                         .arg(media_size.width)
                                         .arg(media_size.height)
                                         .arg(selected_media_key));
    }

    _updateImageSizeDisplay();
}

void LineInspector::_populateMediaComboBox() {
    _ui->copy_from_media_combo->clear();

    if (!dataManager()) {
        return;
    }

    // Get all MediaData keys
    auto media_keys = dataManager()->getKeys<MediaData>();

    if (media_keys.empty()) {
        _ui->copy_from_media_combo->addItem("No media data available");
        _ui->copy_from_media_combo->setEnabled(false);
        return;
    }

    _ui->copy_from_media_combo->setEnabled(true);

    for (auto const & key: media_keys) {
        _ui->copy_from_media_combo->addItem(QString::fromStdString(key));
    }
}

void LineInspector::_onGroupFilterChanged(int index) {
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

void LineInspector::_onGroupChanged() {
    // Store current selection and current text (in case index changes)
    int current_index = _ui->groupFilterCombo->currentIndex();
    QString current_text;
    if (current_index >= 0 && current_index < _ui->groupFilterCombo->count()) {
        current_text = _ui->groupFilterCombo->itemText(current_index);
    }

    // Update the group filter combo box when groups change
    _populateGroupFilterCombo();

    // Restore selection
    restoreGroupFilterSelection(_ui->groupFilterCombo, current_index, current_text);
}

void LineInspector::_populateGroupFilterCombo() {
    populateGroupFilterCombo(_ui->groupFilterCombo, groupManager());
}

void LineInspector::_onAutoScrollToCurrentFrame() {
    if (!_data_view || !dataManager()) {
        return;
    }

    auto const current_position = state()->current_position;
    auto line_data = dataManager()->getData<LineData>(_active_key);
    if (!line_data) {
        return;
    }
    auto current_time = current_position.convertTo(line_data->getTimeFrame().get());

    _data_view->scrollToFrame(current_time.getValue());
}

void LineInspector::setDataView(LineTableView * view) {
    // Disconnect from old view if any
    if (_data_view) {
        disconnect(_data_view, &LineTableView::moveLinesRequested, this, nullptr);
        disconnect(_data_view, &LineTableView::copyLinesRequested, this, nullptr);
        disconnect(_data_view, &LineTableView::moveLinesToGroupRequested, this, nullptr);
        disconnect(_data_view, &LineTableView::removeLinesFromGroupRequested, this, nullptr);
        disconnect(_data_view, &LineTableView::deleteLinesRequested, this, nullptr);
    }

    _data_view = view;
    if (_data_view) {
        if (groupManager()) {
            _data_view->setGroupManager(groupManager());
        }
        
        // Connect to view signals for move/copy operations
        connect(_data_view, &LineTableView::moveLinesRequested,
                this, &LineInspector::_onMoveLinesRequested);
        connect(_data_view, &LineTableView::copyLinesRequested,
                this, &LineInspector::_onCopyLinesRequested);
        
        // Connect to view signals for group management operations
        connect(_data_view, &LineTableView::moveLinesToGroupRequested,
                this, &LineInspector::_onMoveLinesToGroupRequested);
        connect(_data_view, &LineTableView::removeLinesFromGroupRequested,
                this, &LineInspector::_onRemoveLinesFromGroupRequested);
        
        // Connect to view signal for delete operation
        connect(_data_view, &LineTableView::deleteLinesRequested,
                this, &LineInspector::_onDeleteLinesRequested);
    }
}

void LineInspector::_onMoveLinesRequested(std::string const & target_key) {
    if (!_data_view || _active_key.empty()) {
        return;
    }

    auto selected_entity_ids = _data_view->getSelectedEntityIds();
    if (selected_entity_ids.empty()) {
        std::cout << "LineInspector: No lines selected to move." << std::endl;
        return;
    }

    auto source_line_data = dataManager()->getData<LineData>(_active_key);
    auto target_line_data = dataManager()->getData<LineData>(target_key);

    if (!source_line_data) {
        std::cerr << "LineInspector: Source LineData object ('" << _active_key << "') not found." << std::endl;
        return;
    }
    if (!target_line_data) {
        std::cerr << "LineInspector: Target LineData object ('" << target_key << "') not found." << std::endl;
        return;
    }

    std::cout << "LineInspector: Moving " << selected_entity_ids.size()
              << " selected lines from '" << _active_key << "' to '" << target_key << "'..." << std::endl;

    // Use the moveByEntityIds method to move only the selected lines
    std::unordered_set<EntityId> const selected_entity_ids_set(selected_entity_ids.begin(), selected_entity_ids.end());
    std::size_t const total_lines_moved = source_line_data->moveByEntityIds(*target_line_data, selected_entity_ids_set, NotifyObservers::Yes);

    if (total_lines_moved > 0) {
        // Update the view to reflect changes
        _data_view->updateView();

        std::cout << "LineInspector: Successfully moved " << total_lines_moved
                  << " selected lines." << std::endl;
    } else {
        std::cout << "LineInspector: No lines found with the selected EntityIds to move." << std::endl;
    }
}

void LineInspector::_onCopyLinesRequested(std::string const & target_key) {
    if (!_data_view || _active_key.empty()) {
        return;
    }

    auto selected_entity_ids = _data_view->getSelectedEntityIds();
    if (selected_entity_ids.empty()) {
        std::cout << "LineInspector: No lines selected to copy." << std::endl;
        return;
    }

    auto source_line_data = dataManager()->getData<LineData>(_active_key);
    auto target_line_data = dataManager()->getData<LineData>(target_key);

    if (!source_line_data) {
        std::cerr << "LineInspector: Source LineData object ('" << _active_key << "') not found." << std::endl;
        return;
    }
    if (!target_line_data) {
        std::cerr << "LineInspector: Target LineData object ('" << target_key << "') not found." << std::endl;
        return;
    }

    std::cout << "LineInspector: Copying " << selected_entity_ids.size()
              << " selected lines from '" << _active_key << "' to '" << target_key << "'..." << std::endl;

    // Use the copyByEntityIds method to copy only the selected lines
    std::unordered_set<EntityId> const selected_entity_ids_set(selected_entity_ids.begin(), selected_entity_ids.end());
    std::size_t const total_lines_copied = source_line_data->copyByEntityIds(*target_line_data, selected_entity_ids_set, NotifyObservers::Yes);

    if (total_lines_copied > 0) {
        std::cout << "LineInspector: Successfully copied " << total_lines_copied
                  << " selected lines." << std::endl;
    } else {
        std::cout << "LineInspector: No lines found with the selected EntityIds to copy." << std::endl;
    }
}

void LineInspector::_onMoveLinesToGroupRequested(int group_id) {
    if (!_data_view || !groupManager()) {
        return;
    }

    auto selected_entity_ids = _data_view->getSelectedEntityIds();
    if (selected_entity_ids.empty()) {
        std::cout << "LineInspector: No lines selected to move to group." << std::endl;
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

    std::cout << "LineInspector: Moved " << selected_entity_ids.size()
              << " selected lines to group " << group_id << std::endl;
}

void LineInspector::_onRemoveLinesFromGroupRequested() {
    if (!_data_view || !groupManager()) {
        return;
    }

    auto selected_entity_ids = _data_view->getSelectedEntityIds();
    if (selected_entity_ids.empty()) {
        std::cout << "LineInspector: No lines selected to remove from group." << std::endl;
        return;
    }

    std::unordered_set<EntityId> entity_ids_set(selected_entity_ids.begin(), selected_entity_ids.end());

    // Remove entities from all groups
    groupManager()->ungroupEntities(entity_ids_set);

    // Refresh the view to show updated group information
    if (_data_view) {
        _data_view->updateView();
    }

    std::cout << "LineInspector: Removed " << selected_entity_ids.size()
              << " selected lines from their groups." << std::endl;
}

void LineInspector::_onDeleteLinesRequested() {
    if (!_data_view || _active_key.empty()) {
        return;
    }

    auto selected_entity_ids = _data_view->getSelectedEntityIds();
    if (selected_entity_ids.empty()) {
        std::cout << "LineInspector: No lines selected to delete." << std::endl;
        return;
    }

    auto line_data = dataManager()->getData<LineData>(_active_key);
    if (!line_data) {
        std::cerr << "LineInspector: LineData object ('" << _active_key << "') not found for deletion." << std::endl;
        return;
    }

    std::cout << "LineInspector: Deleting " << selected_entity_ids.size()
              << " selected lines from '" << _active_key << "'..." << std::endl;

    // Remove entities from groups first
    if (groupManager()) {
        std::unordered_set<EntityId> entity_ids_set(selected_entity_ids.begin(), selected_entity_ids.end());
        groupManager()->ungroupEntities(entity_ids_set);
    }

    int total_lines_deleted = 0;

    // Delete each selected line individually
    for (EntityId const entity_id : selected_entity_ids) {
        if (entity_id != EntityId(0)) {
            bool const success = line_data->clearByEntityId(entity_id, NotifyObservers::No);
            if (success) {
                total_lines_deleted++;
            }
        }
    }

    // Notify observers only once at the end
    if (total_lines_deleted > 0) {
        line_data->notifyObservers();

        // Update the view to reflect changes
        _data_view->updateView();

        std::cout << "LineInspector: Successfully deleted " << total_lines_deleted
                  << " selected lines." << std::endl;
    } else {
        std::cout << "LineInspector: No lines found to delete from the selected rows." << std::endl;
    }
}
