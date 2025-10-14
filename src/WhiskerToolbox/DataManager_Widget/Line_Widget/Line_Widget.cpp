#include "Line_Widget.hpp"
#include "ui_Line_Widget.h"

#include "DataManager.hpp"
#include "DataManager/ConcreteDataFactory.hpp"
#include "DataManager/IO/LoaderRegistry.hpp"
#include "DataManager/IO/interface/IOTypes.hpp"
#include "DataManager/Lines/Line_Data.hpp"
#include "DataManager/Media/Media_Data.hpp"
#include "LineTableModel.hpp"

#include "DataManager_Widget/utils/DataManager_Widget_utils.hpp"
#include "IO_Widgets/Lines/Binary/BinaryLineSaver_Widget.hpp"
#include "IO_Widgets/Lines/CSV/CSVLineSaver_Widget.hpp"
#include "MediaExport/MediaExport_Widget.hpp"
#include "WhiskerToolbox/GroupManagementWidget/GroupManager.hpp"
// Media export functions
#include "MediaExport/media_export.hpp"

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
#include <iostream>
#include <unordered_set>
#include <set>

Line_Widget::Line_Widget(std::shared_ptr<DataManager> data_manager, QWidget * parent)
    : QWidget(parent),
      ui(new Ui::Line_Widget),
      _data_manager{std::move(data_manager)} {
    ui->setupUi(this);

    _line_table_model = new LineTableModel(this);
    ui->tableView->setModel(_line_table_model);
    ui->tableView->setSelectionBehavior(QAbstractItemView::SelectRows);
    ui->tableView->setEditTriggers(QAbstractItemView::NoEditTriggers);
    ui->tableView->setContextMenuPolicy(Qt::CustomContextMenu);

    connect(ui->tableView, &QTableView::doubleClicked, this, &Line_Widget::_handleCellDoubleClicked);
    connect(ui->tableView, &QTableView::customContextMenuRequested, this, &Line_Widget::_showContextMenu);

    connect(ui->export_type_combo, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &Line_Widget::_onExportTypeChanged);
    connect(ui->csv_line_saver_widget, &CSVLineSaver_Widget::saveCSVRequested,
            this, &Line_Widget::_handleSaveCSVRequested);
    connect(ui->csv_line_saver_widget, &CSVLineSaver_Widget::saveMultiFileCSVRequested,
            this, &Line_Widget::_handleSaveMultiFileCSVRequested);
    connect(ui->binary_line_saver_widget, &BinaryLineSaver_Widget::saveBinaryRequested,
            this, &Line_Widget::_handleSaveBinaryRequested);
    connect(ui->export_media_frames_checkbox, &QCheckBox::toggled,
            this, &Line_Widget::_onExportMediaFramesCheckboxToggled);
    connect(ui->apply_image_size_button, &QPushButton::clicked,
            this, &Line_Widget::_onApplyImageSizeClicked);
    connect(ui->copy_image_size_button, &QPushButton::clicked,
            this, &Line_Widget::_onCopyImageSizeClicked);
    connect(ui->groupFilterCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &Line_Widget::_onGroupFilterChanged);
    connect(ui->autoScrollButton, &QPushButton::clicked,
            this, &Line_Widget::_onAutoScrollToCurrentFrame);

    // Setup collapsible export section
    ui->export_section->autoSetContentLayout();
    ui->export_section->setTitle("Export Options");
    ui->export_section->toggle(false);// Start collapsed

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

Line_Widget::~Line_Widget() {
    // Remove the line data callback
    removeCallbacks();

    delete ui;
}

void Line_Widget::openWidget() {
    this->show();
    // Update image size display when widget is opened
    _updateImageSizeDisplay();
}

void Line_Widget::setActiveKey(std::string const & key) {
    if (_active_key == key && _callback_id != -1) {
        return;
    }

    removeCallbacks();

    _active_key = key;

    auto line_data = _data_manager->getData<LineData>(_active_key);
    if (line_data) {
        _line_table_model->setLines(line_data.get());
        _callback_id = _data_manager->addCallbackToData(_active_key, [this]() { _onDataChanged(); });
        _updateImageSizeDisplay();
    } else {
        _line_table_model->setLines(nullptr);
        std::cerr << "Line_Widget: Could not find LineData with key: " << _active_key << std::endl;
    }
    updateTable();
}

void Line_Widget::removeCallbacks() {
    remove_callback(_data_manager.get(), _active_key, _callback_id);
}

void Line_Widget::updateTable() {
    if (!_active_key.empty()) {
        auto line_data = _data_manager->getData<LineData>(_active_key);
        _line_table_model->setLines(line_data.get());
    } else {
        _line_table_model->setLines(nullptr);
    }
}

void Line_Widget::_handleCellDoubleClicked(QModelIndex const & index) {
    if (!index.isValid()) {
        return;
    }
    LineTableRow const rowData = _line_table_model->getRowData(index.row());
    if (rowData.frame != -1) {
        emit frameSelected(static_cast<int>(rowData.frame));
    }
}

void Line_Widget::_onDataChanged() {
    updateTable();
}

void Line_Widget::_showContextMenu(QPoint const & position) {
    QModelIndex const index = ui->tableView->indexAt(position);
    if (!index.isValid()) {
        return;
    }

    QMenu context_menu(this);

    // Add move and copy submenus using the utility function
    auto move_callback = [this](std::string const & target_key) {
        _moveLineToTarget(target_key);
    };

    auto copy_callback = [this](std::string const & target_key) {
        _copyLineToTarget(target_key);
    };

    add_move_copy_submenus<LineData>(&context_menu, _data_manager.get(), _active_key, move_callback, copy_callback);

    // Add group management options
    context_menu.addSeparator();
    QMenu * group_menu = context_menu.addMenu("Group Management");

    // Add "Move to Group" submenu
    QMenu * move_to_group_menu = group_menu->addMenu("Move to Group");
    _populateGroupSubmenu(move_to_group_menu, true);

    // Add "Remove from Group" action
    QAction * remove_from_group_action = group_menu->addAction("Remove from Group");
    connect(remove_from_group_action, &QAction::triggered, this, &Line_Widget::_removeSelectedLinesFromGroup);

    // Add separator and existing operations
    context_menu.addSeparator();
    QAction * delete_action = context_menu.addAction("Delete Selected Line");

    connect(delete_action, &QAction::triggered, this, &Line_Widget::_deleteSelectedLine);

    context_menu.exec(ui->tableView->mapToGlobal(position));
}

std::vector<TimeFrameIndex> Line_Widget::_getSelectedFrames() {
    QModelIndexList const selectedIndexes = ui->tableView->selectionModel()->selectedRows();
    std::set<int> unique_frames;

    for (auto const & index: selectedIndexes) {
        if (index.isValid()) {
            LineTableRow const row_data = _line_table_model->getRowData(index.row());
            if (row_data.frame != -1) {
                unique_frames.insert(static_cast<int>(row_data.frame));
            }
        }
    }

    return {unique_frames.begin(), unique_frames.end()};
}

std::vector<EntityId> Line_Widget::_getSelectedEntityIds() {
    QModelIndexList const selectedIndexes = ui->tableView->selectionModel()->selectedRows();
    std::vector<EntityId> entity_ids;

    for (auto const & index: selectedIndexes) {
        if (index.isValid()) {
            LineTableRow const row_data = _line_table_model->getRowData(index.row());
            if (row_data.entity_id != 0) {// Valid entity ID
                entity_ids.push_back(row_data.entity_id);
            }
        }
    }

    return entity_ids;
}

void Line_Widget::_moveLineToTarget(std::string const & target_key) {
    auto selected_entity_ids = _getSelectedEntityIds();
    if (selected_entity_ids.empty()) {
        std::cout << "Line_Widget: No lines selected to move." << std::endl;
        return;
    }

    auto source_line_data = _data_manager->getData<LineData>(_active_key);
    auto target_line_data = _data_manager->getData<LineData>(target_key);

    if (!source_line_data) {
        std::cerr << "Line_Widget: Source LineData object ('" << _active_key << "') not found." << std::endl;
        return;
    }
    if (!target_line_data) {
        std::cerr << "Line_Widget: Target LineData object ('" << target_key << "') not found." << std::endl;
        return;
    }

    std::cout << "Line_Widget: Moving " << selected_entity_ids.size()
              << " selected lines from '" << _active_key << "' to '" << target_key << "'..." << std::endl;

    // Use the moveLinesByEntityIds method to move only the selected lines
    std::unordered_set<EntityId> const selected_entity_ids_set(selected_entity_ids.begin(), selected_entity_ids.end());
    std::size_t const total_lines_moved = source_line_data->moveByEntityIds(*target_line_data, selected_entity_ids_set, true);

    if (total_lines_moved > 0) {
        // Update the table view to reflect changes
        updateTable();

        std::cout << "Line_Widget: Successfully moved " << total_lines_moved
                  << " selected lines." << std::endl;
    } else {
        std::cout << "Line_Widget: No lines found with the selected EntityIds to move." << std::endl;
    }
}

void Line_Widget::_copyLineToTarget(std::string const & target_key) {
    auto selected_entity_ids = _getSelectedEntityIds();
    if (selected_entity_ids.empty()) {
        std::cout << "Line_Widget: No lines selected to copy." << std::endl;
        return;
    }

    auto source_line_data = _data_manager->getData<LineData>(_active_key);
    auto target_line_data = _data_manager->getData<LineData>(target_key);

    if (!source_line_data) {
        std::cerr << "Line_Widget: Source LineData object ('" << _active_key << "') not found." << std::endl;
        return;
    }
    if (!target_line_data) {
        std::cerr << "Line_Widget: Target LineData object ('" << target_key << "') not found." << std::endl;
        return;
    }

    std::cout << "Line_Widget: Copying " << selected_entity_ids.size()
              << " selected lines from '" << _active_key << "' to '" << target_key << "'..." << std::endl;

    // Use the copyLinesByEntityIds method to copy only the selected lines
    std::size_t const total_lines_copied = source_line_data->copyLinesByEntityIds(*target_line_data, selected_entity_ids, true);

    if (total_lines_copied > 0) {
        std::cout << "Line_Widget: Successfully copied " << total_lines_copied
                  << " selected lines." << std::endl;
    } else {
        std::cout << "Line_Widget: No lines found with the selected EntityIds to copy." << std::endl;
    }
}

void Line_Widget::_deleteSelectedLine() {
    QModelIndexList selectedIndexes = ui->tableView->selectionModel()->selectedIndexes();
    if (selectedIndexes.isEmpty()) {
        std::cout << "Line_Widget: No line selected to delete." << std::endl;
        return;
    }
    int const selected_row = selectedIndexes.first().row();
    LineTableRow const row_data = _line_table_model->getRowData(selected_row);

    if (row_data.frame == -1) {
        std::cout << "Line_Widget: Selected row data for deletion is invalid." << std::endl;
        return;
    }

    auto source_line_data = _data_manager->getData<LineData>(_active_key);
    if (!source_line_data) {
        std::cerr << "Line_Widget: Source LineData object ('" << _active_key << "') not found for deletion." << std::endl;
        return;
    }

    std::vector<Line2D> const & lines_at_frame = source_line_data->getAtTime(TimeFrameIndex(row_data.frame));
    if (row_data.lineIndex < 0 || static_cast<size_t>(row_data.lineIndex) >= lines_at_frame.size()) {
        std::cerr << "Line_Widget: Line index out of bounds for deletion. Frame: " << row_data.frame
                  << ", Index: " << row_data.lineIndex << std::endl;
        updateTable();
        return;
    }

    bool const clear_success = source_line_data->clearAtTime(TimeFrameIndex(row_data.frame), row_data.lineIndex);
    if (!clear_success) {
        std::cerr << "Line_Widget: Failed to clear line at frame " << row_data.frame
                  << ", index " << row_data.lineIndex << std::endl;
    }

    updateTable();

    std::cout << "Line deleted from " << _active_key << " frame " << row_data.frame << " index " << row_data.lineIndex << std::endl;
}

void Line_Widget::_onExportTypeChanged(int index) {
    QString const current_text = ui->export_type_combo->itemText(index);
    if (current_text == "CSV") {
        ui->stacked_saver_options->setCurrentWidget(ui->csv_line_saver_widget);
    } else if (current_text == "Binary") {
        ui->stacked_saver_options->setCurrentWidget(ui->binary_line_saver_widget);
    } else {
        // Potentially handle other types or clear/hide the stacked widget
    }
}

void Line_Widget::_handleSaveCSVRequested(QString const & format, nlohmann::json const & config) {
    _initiateSaveProcess(format, config);
}

void Line_Widget::_handleSaveMultiFileCSVRequested(QString const & format, nlohmann::json const & config) {
    _initiateSaveProcess(format, config);
}

void Line_Widget::_handleSaveBinaryRequested(QString const & format, nlohmann::json const & config) {
    _initiateSaveProcess(format, config);
}

void Line_Widget::_onExportMediaFramesCheckboxToggled(bool checked) {
    ui->media_export_options_widget->setVisible(checked);
}

void Line_Widget::_initiateSaveProcess(QString const & format, LineSaverConfig const & config) {
    if (_active_key.empty()) {
        QMessageBox::warning(this, "No Data Selected", "Please select a LineData item to save.");
        return;
    }

    auto line_data_ptr = _data_manager->getData<LineData>(_active_key);
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
            updated_config["parent_dir"] = _data_manager->getOutputPath().string();
        } else {
            updated_config["parent_dir"] = _data_manager->getOutputPath().string() + "/" + parent_dir;
        }
    }
    // If parent_dir is absolute, use it as-is

    bool const save_successful = _performRegistrySave(format, updated_config);

    if (!save_successful) {
        return;
    }

    if (ui->export_media_frames_checkbox->isChecked()) {
        auto times_with_data = line_data_ptr->getTimesWithData();
        std::vector<size_t> frame_ids_to_export;
        frame_ids_to_export.reserve(times_with_data.size());
        for (auto frame_id: times_with_data) {
            frame_ids_to_export.push_back(static_cast<size_t>(frame_id.getValue()));
        }

        if (frame_ids_to_export.empty()) {
            QMessageBox::information(this, "No Frames", "No lines found in data, so no media frames to export.");
        } else {
            auto media_ptr = _data_manager->getData<MediaData>("media");
            if (!media_ptr) {
                QMessageBox::warning(this, "Media Not Available", "Could not access media for exporting frames.");
                return;
            }

            MediaExportOptions options = ui->media_export_options_widget->getOptions();
            // Use the same parent directory used for line saving
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

bool Line_Widget::_performRegistrySave(QString const & format, LineSaverConfig const & config) {
    auto line_data_ptr = _data_manager->getData<LineData>(_active_key);
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

void Line_Widget::_onApplyImageSizeClicked() {
    if (_active_key.empty()) {
        QMessageBox::warning(this, "No Data Selected", "Please select a LineData item to modify image size.");
        return;
    }

    auto line_data = _data_manager->getData<LineData>(_active_key);
    if (!line_data) {
        QMessageBox::critical(this, "Error", "Could not retrieve LineData for image size modification. Key: " + QString::fromStdString(_active_key));
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

void Line_Widget::_updateImageSizeDisplay() {
    if (_active_key.empty()) {
        ui->image_width_edit->setText("");
        ui->image_height_edit->setText("");
        ui->image_size_status_label->setText("No Data Selected");
        ui->image_size_status_label->setStyleSheet("color: #666666; font-style: italic;");
        std::cout << "Line_Widget::_updateImageSizeDisplay: No active key" << std::endl;
        return;
    }

    auto line_data = _data_manager->getData<LineData>(_active_key);
    if (!line_data) {
        ui->image_width_edit->setText("");
        ui->image_height_edit->setText("");
        ui->image_size_status_label->setText("Data Not Found");
        ui->image_size_status_label->setStyleSheet("color: #cc0000; font-style: italic;");
        std::cout << "Line_Widget::_updateImageSizeDisplay: No line data found for key: " << _active_key << std::endl;
        return;
    }

    ImageSize current_size = line_data->getImageSize();
    std::cout << "Line_Widget::_updateImageSizeDisplay: Current size: " << current_size.width << " x " << current_size.height << std::endl;

    if (current_size.width == -1 || current_size.height == -1) {
        ui->image_width_edit->setText("");
        ui->image_height_edit->setText("");
        ui->image_size_status_label->setText("Not Set");
        ui->image_size_status_label->setStyleSheet("color: #666666; font-style: italic;");
        std::cout << "Line_Widget::_updateImageSizeDisplay: No size set, clearing fields" << std::endl;
    } else {
        ui->image_width_edit->setText(QString::number(current_size.width));
        ui->image_height_edit->setText(QString::number(current_size.height));
        ui->image_size_status_label->setText(QString("%1 × %2").arg(current_size.width).arg(current_size.height));
        ui->image_size_status_label->setStyleSheet("color: #000000; font-weight: bold;");
        std::cout << "Line_Widget::_updateImageSizeDisplay: Set fields to " << current_size.width << " x " << current_size.height << std::endl;
    }
}

void Line_Widget::_onCopyImageSizeClicked() {
    if (_active_key.empty()) {
        QMessageBox::warning(this, "No Data Selected", "Please select a LineData item to modify image size.");
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

    auto line_data = _data_manager->getData<LineData>(_active_key);
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

void Line_Widget::_populateMediaComboBox() {
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

    for (auto const & key: media_keys) {
        ui->copy_from_media_combo->addItem(QString::fromStdString(key));
    }

    std::cout << "Line_Widget::_populateMediaComboBox: Found " << media_keys.size() << " media keys" << std::endl;
}

void Line_Widget::setGroupManager(GroupManager * group_manager) {
    _group_manager = group_manager;
    _line_table_model->setGroupManager(group_manager);
    _populateGroupFilterCombo();

    // Connect to group manager signals to update when groups change
    if (_group_manager) {
        connect(_group_manager, &GroupManager::groupCreated,
                this, &Line_Widget::_onGroupChanged);
        connect(_group_manager, &GroupManager::groupRemoved,
                this, &Line_Widget::_onGroupChanged);
        connect(_group_manager, &GroupManager::groupModified,
                this, &Line_Widget::_onGroupChanged);
    }
}

void Line_Widget::_onGroupFilterChanged(int index) {
    if (!_group_manager) {
        return;
    }

    if (index == 0) {
        // "All Groups" selected
        _line_table_model->clearGroupFilter();
    } else {
        // Specific group selected (index - 1 because index 0 is "All Groups")
        auto groups = _group_manager->getGroups();
        auto group_ids = groups.keys();
        if (index - 1 < group_ids.size()) {
            int group_id = group_ids[index - 1];
            _line_table_model->setGroupFilter(group_id);
        }
    }
}

void Line_Widget::_onGroupChanged() {
    // Store current selection
    int current_index = ui->groupFilterCombo->currentIndex();

    // Update the group filter combo box when groups change
    _populateGroupFilterCombo();

    // If the previously selected group no longer exists, reset to "All Groups"
    if (current_index > 0 && current_index >= ui->groupFilterCombo->count()) {
        ui->groupFilterCombo->setCurrentIndex(0);// "All Groups"
        _line_table_model->clearGroupFilter();
    }

    // Refresh the table to update group names
    if (!_active_key.empty()) {
        updateTable();
    }
}

void Line_Widget::_populateGroupFilterCombo() {
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

void Line_Widget::_populateGroupSubmenu(QMenu * menu, bool for_moving) {
    if (!_group_manager) {
        return;
    }

    // Get current groups of selected entities to exclude them from the move list
    std::set<int> current_groups;
    if (for_moving) {
        QModelIndexList selectedIndexes = ui->tableView->selectionModel()->selectedRows();
        for (auto const & index: selectedIndexes) {
            LineTableRow const row_data = _line_table_model->getRowData(index.row());
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
            _moveSelectedLinesToGroup(group_id);
        });
    }
}

void Line_Widget::_moveSelectedLinesToGroup(int group_id) {
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
    for (auto const & index: selectedIndexes) {
        LineTableRow const row_data = _line_table_model->getRowData(index.row());
        if (row_data.entity_id != 0) {// Valid entity ID
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

void Line_Widget::_removeSelectedLinesFromGroup() {
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
    for (auto const & index: selectedIndexes) {
        LineTableRow const row_data = _line_table_model->getRowData(index.row());
        if (row_data.entity_id != 0) {// Valid entity ID
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

void Line_Widget::_onAutoScrollToCurrentFrame() {
    if (!_data_manager) {
        return;
    }

    int64_t current_frame = _data_manager->getCurrentTime();
    int row_index = _line_table_model->findRowForFrame(current_frame);

    if (row_index >= 0) {
        // Scroll to the found row
        QModelIndex index = _line_table_model->index(row_index, 0);
        ui->tableView->scrollTo(index, QAbstractItemView::PositionAtCenter);

        // Optionally select the row
        ui->tableView->selectRow(row_index);

        std::cout << "Line_Widget: Scrolled to frame " << current_frame << " at row " << row_index << std::endl;
    } else {
        std::cout << "Line_Widget: No data found for current frame " << current_frame << std::endl;
    }
}
