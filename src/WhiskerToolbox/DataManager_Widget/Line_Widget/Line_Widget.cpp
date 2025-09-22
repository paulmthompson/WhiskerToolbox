#include "Line_Widget.hpp"
#include "ui_Line_Widget.h"

#include "DataManager.hpp"
#include "DataManager/Lines/Line_Data.hpp"
#include "DataManager/Media/Media_Data.hpp"
#include "DataManager/IO/LoaderRegistry.hpp"
#include "DataManager/IO/interface/IOTypes.hpp"
#include "DataManager/ConcreteDataFactory.hpp"
#include "LineTableModel.hpp"

#include "DataManager_Widget/utils/DataManager_Widget_utils.hpp"
#include "IO_Widgets/Lines/Binary/BinaryLineSaver_Widget.hpp"
#include "IO_Widgets/Lines/CSV/CSVLineSaver_Widget.hpp"
#include "MediaExport/MediaExport_Widget.hpp"
// Media export functions
#include "MediaExport/media_export.hpp"

#include <QCheckBox>
#include <QComboBox>
#include <QDir>
#include <QFileDialog>
#include <QMenu>
#include <QMessageBox>
#include <QPushButton>
#include <QStackedWidget>
#include <QTableView>

#include <filesystem>
#include <iostream>
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

    _onExportTypeChanged(ui->export_type_combo->currentIndex());
    ui->media_export_options_widget->setVisible(ui->export_media_frames_checkbox->isChecked());
}

Line_Widget::~Line_Widget() {
    delete ui;
}

void Line_Widget::openWidget() {
    this->show();
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
        emit frameSelected(rowData.frame);
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
                unique_frames.insert(row_data.frame);
            }
        }
    }

    return {unique_frames.begin(), unique_frames.end()};
}

void Line_Widget::_moveLineToTarget(std::string const & target_key) {
   auto selected_frames = _getSelectedFrames();
    if (selected_frames.empty()) {
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

    std::cout << "Line_Widget: Moving lines from " << selected_frames.size()
              << " frames from '" << _active_key << "' to '" << target_key << "'..." << std::endl;

    // Use the new moveTo method with the vector of selected times
    std::size_t const total_lines_moved = source_line_data->moveTo(*target_line_data, selected_frames, true);

    if (total_lines_moved > 0) {
        // Update the table view to reflect changes
        updateTable();

        std::cout << "Line_Widget: Successfully moved " << total_lines_moved
                  << " lines from " << selected_frames.size() << " frames." << std::endl;
    } else {
        std::cout << "Line_Widget: No lines found in any of the selected frames to move." << std::endl;
    }
}

void Line_Widget::_copyLineToTarget(std::string const & target_key) {
    auto selected_frames = _getSelectedFrames();
    if (selected_frames.empty()) {
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

    std::cout << "Line_Widget: Copying lines from " << selected_frames.size()
              << " frames from '" << _active_key << "' to '" << target_key << "'..." << std::endl;

    // Use the new copyTo method with the vector of selected times
    std::size_t const total_lines_copied = source_line_data->copyTo(*target_line_data, selected_frames, true);

    if (total_lines_copied > 0) {
        std::cout << "Line_Widget: Successfully copied " << total_lines_copied
                  << " lines from " << selected_frames.size() << " frames." << std::endl;
    } else {
        std::cout << "Line_Widget: No lines found in any of the selected frames to copy." << std::endl;
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

    source_line_data->clearAtTime(TimeFrameIndex(row_data.frame), row_data.lineIndex);

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

void Line_Widget::_initiateSaveProcess(QString const& format, LineSaverConfig const& config) {
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
    }
}

bool Line_Widget::_performRegistrySave(QString const& format, LineSaverConfig const& config) {
    auto line_data_ptr = _data_manager->getData<LineData>(_active_key);
    if (!line_data_ptr) {
        QMessageBox::critical(this, "Save Error", "Critical: Could not retrieve LineData for saving. Key: " + QString::fromStdString(_active_key));
        return false;
    }

    // Check if format is supported through registry
    LoaderRegistry& registry = LoaderRegistry::getInstance();
    std::string const format_str = format.toStdString();
    
    if (!registry.isFormatSupported(format_str, IODataType::Line)) {
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
