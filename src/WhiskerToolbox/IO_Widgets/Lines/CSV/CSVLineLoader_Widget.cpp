#include "CSVLineLoader_Widget.hpp"

#include "ui_CSVLineLoader_Widget.h"

#include <QPushButton>
#include <QFileDialog>
#include <QRadioButton>
#include <QLineEdit>
#include <QComboBox>
#include <QSpinBox>
#include <QCheckBox>
#include <QLabel>
#include <QMessageBox>

CSVLineLoader_Widget::CSVLineLoader_Widget(QWidget * parent)
    : QWidget(parent),
      ui(new Ui::CSVLineLoader_Widget){
    ui->setupUi(this);

    // Connect signals
    connect(ui->single_file_radio, &QRadioButton::toggled, this, &CSVLineLoader_Widget::_onLoadModeChanged);
    connect(ui->multi_file_radio, &QRadioButton::toggled, this, &CSVLineLoader_Widget::_onLoadModeChanged);
    connect(ui->browse_button, &QPushButton::clicked, this, &CSVLineLoader_Widget::_onBrowseButtonClicked);
    connect(ui->load_button, &QPushButton::clicked, this, &CSVLineLoader_Widget::_onLoadButtonClicked);

    // Initialize UI state
    _updateUIForLoadMode();
}

CSVLineLoader_Widget::~CSVLineLoader_Widget() {
    delete ui;
}

void CSVLineLoader_Widget::_onLoadModeChanged() {
    _updateUIForLoadMode();
}

void CSVLineLoader_Widget::_onBrowseButtonClicked() {
    QString selectedPath;
    
    if (ui->single_file_radio->isChecked()) {
        // Single file mode - open file dialog
        selectedPath = QFileDialog::getOpenFileName(
            this,
            "Select CSV File",
            "",
            "CSV Files (*.csv);;All Files (*)"
        );
    } else {
        // Multi file mode - open directory dialog
        selectedPath = QFileDialog::getExistingDirectory(
            this,
            "Select Directory Containing CSV Files",
            "",
            QFileDialog::ShowDirsOnly | QFileDialog::DontResolveSymlinks
        );
    }
    
    if (!selectedPath.isEmpty()) {
        ui->file_path_edit->setText(selectedPath);
    }
}

void CSVLineLoader_Widget::_onLoadButtonClicked() {
    QString filePath = ui->file_path_edit->text().trimmed();
    
    if (filePath.isEmpty()) {
        QMessageBox::warning(this, "No Path Selected", "Please select a file or directory to load.");
        return;
    }
    
    if (ui->single_file_radio->isChecked()) {
        // Single file mode
        CSVSingleFileLineLoaderOptions options;
        options.filepath = filePath.toStdString();
        
        // Get delimiter
        QString delimiterText = ui->delimiter_combo->currentText();
        if (delimiterText == "Comma") {
            options.delimiter = ",";
        } else if (delimiterText == "Space") {
            options.delimiter = " ";
        } else if (delimiterText == "Tab") {
            options.delimiter = "\t";
        } else {
            options.delimiter = ","; // Default
        }
        
        // Get coordinate delimiter
        QString coordDelimiterText = ui->coordinate_delimiter_combo->currentText();
        if (coordDelimiterText == "Comma") {
            options.coordinate_delimiter = ",";
        } else if (coordDelimiterText == "Space") {
            options.coordinate_delimiter = " ";
        } else if (coordDelimiterText == "Tab") {
            options.coordinate_delimiter = "\t";
        } else {
            options.coordinate_delimiter = ","; // Default
        }
        
        options.has_header = ui->has_header_checkbox->isChecked();
        options.header_identifier = ui->header_identifier_edit->text().toStdString();
        
        emit loadSingleFileCSVRequested(options);
    } else {
        // Multi file mode
        CSVMultiFileLineLoaderOptions options;
        options.parent_dir = filePath.toStdString();
        
        // Get delimiter
        QString delimiterText = ui->delimiter_combo->currentText();
        if (delimiterText == "Comma") {
            options.delimiter = ",";
        } else if (delimiterText == "Space") {
            options.delimiter = " ";
        } else if (delimiterText == "Tab") {
            options.delimiter = "\t";
        } else {
            options.delimiter = ","; // Default
        }
        
        options.x_column = ui->x_column_spinbox->value();
        options.y_column = ui->y_column_spinbox->value();
        options.has_header = ui->has_header_checkbox->isChecked();
        
        // Validate column indices
        if (options.x_column == options.y_column) {
            QMessageBox::warning(this, "Invalid Column Configuration", 
                                "X and Y columns cannot be the same. Please select different column indices.");
            return;
        }
        
        emit loadMultiFileCSVRequested(options);
    }
}

void CSVLineLoader_Widget::_updateUIForLoadMode() {
    bool isSingleFile = ui->single_file_radio->isChecked();
    
    // Update label text
    if (isSingleFile) {
        ui->label_file_path->setText("CSV File:");
        ui->file_path_edit->setPlaceholderText("Select CSV file...");
    } else {
        ui->label_file_path->setText("Directory:");
        ui->file_path_edit->setPlaceholderText("Select directory containing CSV files...");
    }
    
    // Column controls are only relevant for multi-file mode
    ui->label_x_column->setEnabled(!isSingleFile);
    ui->x_column_spinbox->setEnabled(!isSingleFile);
    ui->label_y_column->setEnabled(!isSingleFile);
    ui->y_column_spinbox->setEnabled(!isSingleFile);
    
    // Coordinate delimiter and header identifier are only relevant for single-file mode
    ui->label_coordinate_delimiter->setEnabled(isSingleFile);
    ui->coordinate_delimiter_combo->setEnabled(isSingleFile);
    ui->label_header_identifier->setEnabled(isSingleFile);
    ui->header_identifier_edit->setEnabled(isSingleFile);
    
    // Header checkbox is relevant for both modes
    ui->has_header_checkbox->setEnabled(true);
    
    // Clear the path when switching modes
    ui->file_path_edit->clear();
}


