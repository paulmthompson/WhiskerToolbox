#include "Line_Widget.hpp"
#include "ui_Line_Widget.h"

#include "DataManager.hpp"
#include "DataManager/Lines/Line_Data.hpp"
#include "DataManager/Media/Media_Data.hpp"
#include "LineTableModel.hpp"

#include "DataManager_Widget/utils/DataManager_Widget_utils.hpp"
#include "IO_Widgets/Lines/CSV/CSVLineSaver_Widget.hpp"
#include "IO_Widgets/Lines/Binary/BinaryLineSaver_Widget.hpp"
#include "IO_Widgets/Media/MediaExport_Widget.hpp"

#include <QTableView>
#include <QStackedWidget>
#include <QComboBox>
#include <QCheckBox>
#include <QPushButton>
#include <QMessageBox>
#include <QFileDialog>
#include <QDir>

#include <iostream>
#include <filesystem>

Line_Widget::Line_Widget(std::shared_ptr<DataManager> data_manager, QWidget * parent)
    : QWidget(parent),
      ui(new Ui::Line_Widget),
      _data_manager{std::move(data_manager)} {
    ui->setupUi(this);

    _line_table_model = new LineTableModel(this);
    ui->tableView->setModel(_line_table_model);
    ui->tableView->setSelectionBehavior(QAbstractItemView::SelectRows);
    ui->tableView->setEditTriggers(QAbstractItemView::NoEditTriggers);

    connect(ui->tableView, &QTableView::doubleClicked, this, &Line_Widget::_handleCellDoubleClicked);
    connect(ui->moveLineButton, &QPushButton::clicked, this, &Line_Widget::_moveLineButton_clicked);
    connect(ui->deleteLineButton, &QPushButton::clicked, this, &Line_Widget::_deleteLineButton_clicked);

    connect(ui->export_type_combo, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &Line_Widget::_onExportTypeChanged);
    connect(ui->csv_line_saver_widget, &CSVLineSaver_Widget::saveCSVRequested,
            this, &Line_Widget::_handleSaveCSVRequested);
    connect(ui->binary_line_saver_widget, &BinaryLineSaver_Widget::saveBinaryRequested,
            this, &Line_Widget::_handleSaveBinaryRequested);
    connect(ui->export_media_frames_checkbox, &QCheckBox::toggled,
            this, &Line_Widget::_onExportMediaFramesCheckboxToggled);

    _populateMoveToComboBox();

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
    _populateMoveToComboBox();
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

void Line_Widget::_populateMoveToComboBox() {

    populate_move_combo_box<LineData>(ui->moveToComboBox, _data_manager.get(), _active_key);
}

void Line_Widget::_moveLineButton_clicked() {
    QModelIndexList selectedIndexes = ui->tableView->selectionModel()->selectedIndexes();
    if (selectedIndexes.isEmpty()) {
        std::cout << "Line_Widget: No line selected to move." << std::endl;
        return;
    }
    int selected_row = selectedIndexes.first().row();
    LineTableRow row_data = _line_table_model->getRowData(selected_row);

    if (row_data.frame == -1) {
        std::cout << "Line_Widget: Selected row data is invalid." << std::endl;
        return;
    }

    QString target_key_qstr = ui->moveToComboBox->currentText();
    if (target_key_qstr.isEmpty()) {
        std::cout << "Line_Widget: No target selected in ComboBox." << std::endl;
        return;
    }
    std::string target_key = target_key_qstr.toStdString();

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

    std::vector<Line2D> const & lines_at_frame = source_line_data->getLinesAtTime(row_data.frame);
    if (row_data.lineIndex < 0 || static_cast<size_t>(row_data.lineIndex) >= lines_at_frame.size()) {
        std::cerr << "Line_Widget: Line index out of bounds for frame " << row_data.frame << std::endl;
        return;
    }
    Line2D line_to_move = lines_at_frame[row_data.lineIndex];

    target_line_data->addLineAtTime(row_data.frame, line_to_move);

    source_line_data->clearLineAtTime(row_data.frame, row_data.lineIndex);

    updateTable();

    _populateMoveToComboBox();

    std::cout << "Line moved from " << _active_key << " frame " << row_data.frame << " index " << row_data.lineIndex
              << " to " << target_key << std::endl;
}

void Line_Widget::_deleteLineButton_clicked() {
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

    source_line_data->clearLineAtTime(row_data.frame, row_data.lineIndex);

    updateTable();
    _populateMoveToComboBox();

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

void Line_Widget::_handleSaveBinaryRequested(BinaryLineSaverOptions binary_options) {
    LineSaverOptionsVariant options_variant = binary_options;
    _initiateSaveProcess(SaverType::BINARY, options_variant);
}

void Line_Widget::_onExportMediaFramesCheckboxToggled(bool checked) {
    ui->media_export_options_widget->setVisible(checked);
}

void Line_Widget::_initiateSaveProcess(SaverType saver_type, LineSaverOptionsVariant& options_variant) {
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
            CSVSingleFileLineSaverOptions& specific_csv_options = std::get<CSVSingleFileLineSaverOptions>(options_variant);
            specific_csv_options.parent_dir = _data_manager->getOutputPath().string();
            saved_parent_dir = specific_csv_options.parent_dir; // Store for media export
            save_successful = _performActualCSVSave(specific_csv_options);
            break;
        }
        case SaverType::BINARY: {
            BinaryLineSaverOptions& specific_binary_options = std::get<BinaryLineSaverOptions>(options_variant);
            specific_binary_options.parent_dir = _data_manager->getOutputPath().string();
            saved_parent_dir = specific_binary_options.parent_dir; // Store for media export
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
        for(int frame_id : times_with_data_int){
            frame_ids_to_export.push_back(static_cast<size_t>(frame_id));
        }

        if (frame_ids_to_export.empty()){
            QMessageBox::information(this, "No Frames", "No lines found in data, so no media frames to export.");
        } else {
            // Ensure the parent_dir is set in the variant for export_media_frames
            // The utility function expects the parent_dir to be part of the options variant.
            // For CSVSingleFileLineSaverOptions, it's already set.
            // If other option types are added, ensure they also have a parent_dir or adapt the utility.
            export_media_frames(_data_manager.get(),
                                ui->media_export_options_widget,
                                options_variant, // Pass the original variant with parent_dir set
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
        save(line_data_ptr.get(), options); // options.parent_dir and options.filename are used by this function
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
