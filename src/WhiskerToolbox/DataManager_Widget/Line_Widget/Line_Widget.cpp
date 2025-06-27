#include "Line_Widget.hpp"
#include "ui_Line_Widget.h"

#include "DataManager.hpp"
#include "DataManager/Lines/Line_Data.hpp"
#include "DataManager/Media/Media_Data.hpp"
#include "LineTableModel.hpp"

#include "DataManager_Widget/utils/DataManager_Widget_utils.hpp"
#include "IO_Widgets/Lines/Binary/BinaryLineSaver_Widget.hpp"
#include "IO_Widgets/Lines/CSV/CSVLineSaver_Widget.hpp"
#include "IO_Widgets/Media/MediaExport_Widget.hpp"

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
    LineTableRow rowData = _line_table_model->getRowData(index.row());
    if (rowData.frame != -1) {
        emit frameSelected(rowData.frame);
    }
}

void Line_Widget::_onDataChanged() {
    updateTable();
}

void Line_Widget::_showContextMenu(QPoint const & position) {
    QModelIndex index = ui->tableView->indexAt(position);
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

std::vector<int> Line_Widget::_getSelectedFrames() {
    QModelIndexList selectedIndexes = ui->tableView->selectionModel()->selectedRows();
    std::set<int> unique_frames;

    for (auto const & index: selectedIndexes) {
        if (index.isValid()) {
            LineTableRow row_data = _line_table_model->getRowData(index.row());
            if (row_data.frame != -1) {
                unique_frames.insert(row_data.frame);
            }
        }
    }

    return std::vector<int>(unique_frames.begin(), unique_frames.end());
}

void Line_Widget::_moveLineToTarget(std::string const & target_key) {
    std::vector<int> selected_frames = _getSelectedFrames();
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
    std::size_t total_lines_moved = source_line_data->moveTo(*target_line_data, selected_frames, true);

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
    std::vector<int> selected_frames = _getSelectedFrames();
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
    std::size_t total_lines_copied = source_line_data->copyTo(*target_line_data, selected_frames, true);

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
    int selected_row = selectedIndexes.first().row();
    LineTableRow row_data = _line_table_model->getRowData(selected_row);

    if (row_data.frame == -1) {
        std::cout << "Line_Widget: Selected row data for deletion is invalid." << std::endl;
        return;
    }

    auto source_line_data = _data_manager->getData<LineData>(_active_key);
    if (!source_line_data) {
        std::cerr << "Line_Widget: Source LineData object ('" << _active_key << "') not found for deletion." << std::endl;
        return;
    }

    std::vector<Line2D> const & lines_at_frame = source_line_data->getLinesAtTime(row_data.frame);
    if (row_data.lineIndex < 0 || static_cast<size_t>(row_data.lineIndex) >= lines_at_frame.size()) {
        std::cerr << "Line_Widget: Line index out of bounds for deletion. Frame: " << row_data.frame
                  << ", Index: " << row_data.lineIndex << std::endl;
        updateTable();
        return;
    }

    source_line_data->clearLineAtTime(TimeFrameIndex(row_data.frame), row_data.lineIndex);

    updateTable();

    std::cout << "Line deleted from " << _active_key << " frame " << row_data.frame << " index " << row_data.lineIndex << std::endl;
}

void Line_Widget::_onExportTypeChanged(int index) {
    QString current_text = ui->export_type_combo->itemText(index);
    if (current_text == "CSV") {
        ui->stacked_saver_options->setCurrentWidget(ui->csv_line_saver_widget);
    } else if (current_text == "Binary") {
        ui->stacked_saver_options->setCurrentWidget(ui->binary_line_saver_widget);
    } else {
        // Potentially handle other types or clear/hide the stacked widget
    }
}

void Line_Widget::_handleSaveCSVRequested(CSVSingleFileLineSaverOptions csv_options) {
    LineSaverOptionsVariant options_variant = csv_options;
    _initiateSaveProcess(SaverType::CSV, options_variant);
}

void Line_Widget::_handleSaveMultiFileCSVRequested(CSVMultiFileLineSaverOptions csv_options) {
    LineSaverOptionsVariant options_variant = csv_options;
    _initiateSaveProcess(SaverType::CSV, options_variant);
}

void Line_Widget::_handleSaveBinaryRequested(BinaryLineSaverOptions binary_options) {
    LineSaverOptionsVariant options_variant = binary_options;
    _initiateSaveProcess(SaverType::BINARY, options_variant);
}

void Line_Widget::_onExportMediaFramesCheckboxToggled(bool checked) {
    ui->media_export_options_widget->setVisible(checked);
}

void Line_Widget::_initiateSaveProcess(SaverType saver_type, LineSaverOptionsVariant & options_variant) {
    if (_active_key.empty()) {
        QMessageBox::warning(this, "No Data Selected", "Please select a LineData item to save.");
        return;
    }

    auto line_data_ptr = _data_manager->getData<LineData>(_active_key);
    if (!line_data_ptr) {
        QMessageBox::critical(this, "Error", "Could not retrieve LineData for saving. Key: " + QString::fromStdString(_active_key));
        return;
    }

    bool save_successful = false;
    std::string saved_parent_dir;

    switch (saver_type) {
        case SaverType::CSV: {
            if (std::holds_alternative<CSVSingleFileLineSaverOptions>(options_variant)) {
                CSVSingleFileLineSaverOptions & specific_csv_options = std::get<CSVSingleFileLineSaverOptions>(options_variant);
                specific_csv_options.parent_dir = _data_manager->getOutputPath().string();
                saved_parent_dir = specific_csv_options.parent_dir;// Store for media export
                save_successful = _performActualCSVSave(specific_csv_options);
            } else if (std::holds_alternative<CSVMultiFileLineSaverOptions>(options_variant)) {
                CSVMultiFileLineSaverOptions & specific_multi_csv_options = std::get<CSVMultiFileLineSaverOptions>(options_variant);
                specific_multi_csv_options.parent_dir = _data_manager->getOutputPath().string() + "/" + specific_multi_csv_options.parent_dir;
                saved_parent_dir = specific_multi_csv_options.parent_dir;// Store for media export
                save_successful = _performActualMultiFileCSVSave(specific_multi_csv_options);
            }
            break;
        }
        case SaverType::BINARY: {
            BinaryLineSaverOptions & specific_binary_options = std::get<BinaryLineSaverOptions>(options_variant);
            specific_binary_options.parent_dir = _data_manager->getOutputPath().string();
            saved_parent_dir = specific_binary_options.parent_dir;// Store for media export
            save_successful = _performActualBinarySave(specific_binary_options);
            break;
        }
            // Future saver types can be added here
    }

    if (!save_successful) {
        // Error message is shown in _performActualCSVSave or if data ptr is null
        return;
    }

    if (ui->export_media_frames_checkbox->isChecked()) {
        auto times_with_data_int = line_data_ptr->getTimesWithData();
        std::vector<size_t> frame_ids_to_export;
        frame_ids_to_export.reserve(times_with_data_int.size());
        for (int frame_id: times_with_data_int) {
            frame_ids_to_export.push_back(static_cast<size_t>(frame_id));
        }

        if (frame_ids_to_export.empty()) {
            QMessageBox::information(this, "No Frames", "No lines found in data, so no media frames to export.");
        } else {
            // Ensure frame ID padding consistency with multi-file CSV when media export is enabled
            if (std::holds_alternative<CSVMultiFileLineSaverOptions>(options_variant)) {
                auto media_export_opts = ui->media_export_options_widget->getOptions();
                CSVMultiFileLineSaverOptions & multi_csv_opts = std::get<CSVMultiFileLineSaverOptions>(options_variant);
                // Update the media export widget's frame padding to match CSV multi-file padding
                // This ensures consistency between CSV filenames and media frame filenames
                // Note: This is a design choice to keep frame numbering consistent
            }

            export_media_frames(_data_manager.get(),
                                ui->media_export_options_widget,
                                options_variant,// Pass the original variant with parent_dir set
                                this,
                                frame_ids_to_export);
        }
    }
}

bool Line_Widget::_performActualCSVSave(CSVSingleFileLineSaverOptions & options) {
    auto line_data_ptr = _data_manager->getData<LineData>(_active_key);
    if (!line_data_ptr) {
        QMessageBox::critical(this, "Save Error", "Critical: Could not retrieve LineData for saving. Key: " + QString::fromStdString(_active_key));
        return false;
    }

    try {
        save(line_data_ptr.get(), options);// options.parent_dir and options.filename are used by this function
        std::string full_path = options.parent_dir + "/" + options.filename;
        QMessageBox::information(this, "Save Successful", QString::fromStdString("Line data saved to " + full_path));
        std::cout << "Line data saved to: " << full_path << std::endl;
        return true;
    } catch (std::exception const & e) {
        QMessageBox::critical(this, "Save Error", "Failed to save line data: " + QString::fromStdString(e.what()));
        std::cerr << "Failed to save line data: " << e.what() << std::endl;
        return false;
    }
}

bool Line_Widget::_performActualBinarySave(BinaryLineSaverOptions & options) {
    auto line_data_ptr = _data_manager->getData<LineData>(_active_key);
    if (!line_data_ptr) {
        QMessageBox::critical(this, "Save Error", "Critical: Could not retrieve LineData for saving. Key: " + QString::fromStdString(_active_key));
        return false;
    }

    try {
        // The save method already handles parent_dir and filename
        if (save(*line_data_ptr, options)) {
            std::string full_path = options.parent_dir + "/" + options.filename;
            QMessageBox::information(this, "Save Successful", QString::fromStdString("Line data saved to " + full_path));
            std::cout << "Line data saved to: " << full_path << std::endl;
            return true;
        } else {
            QMessageBox::critical(this, "Save Error", "Failed to save line data to binary format. Check console for details.");
            return false;
        }
    } catch (std::exception const & e) {
        QMessageBox::critical(this, "Save Error", "Failed to save line data: " + QString::fromStdString(e.what()));
        std::cerr << "Failed to save line data (binary): " << e.what() << std::endl;
        return false;
    }
}

bool Line_Widget::_performActualMultiFileCSVSave(CSVMultiFileLineSaverOptions & options) {
    auto line_data_ptr = _data_manager->getData<LineData>(_active_key);
    if (!line_data_ptr) {
        QMessageBox::critical(this, "Save Error", "Critical: Could not retrieve LineData for saving. Key: " + QString::fromStdString(_active_key));
        return false;
    }

    try {
        save(line_data_ptr.get(), options);// options.parent_dir is used by this function
        QMessageBox::information(this, "Save Successful", QString::fromStdString("Line data saved to directory: " + options.parent_dir));
        std::cout << "Line data saved to directory: " << options.parent_dir << std::endl;
        return true;
    } catch (std::exception const & e) {
        QMessageBox::critical(this, "Save Error", "Failed to save multi-file line data: " + QString::fromStdString(e.what()));
        std::cerr << "Failed to save multi-file line data: " << e.what() << std::endl;
        return false;
    }
}
