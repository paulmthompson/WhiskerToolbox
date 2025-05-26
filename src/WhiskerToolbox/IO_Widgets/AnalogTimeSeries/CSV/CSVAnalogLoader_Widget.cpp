#include "CSVAnalogLoader_Widget.hpp"
#include "ui_CSVAnalogLoader_Widget.h"

#include <QFileDialog>
#include <QLineEdit>
#include <QComboBox>
#include <QSpinBox>
#include <QCheckBox>
#include <QPushButton>
#include <QLabel>
#include <QMessageBox>

CSVAnalogLoader_Widget::CSVAnalogLoader_Widget(QWidget *parent)
    : QWidget(parent),
      ui(new Ui::CSVAnalogLoader_Widget)
{
    ui->setupUi(this);

    // Connect signals
    connect(ui->browse_button, &QPushButton::clicked, 
            this, &CSVAnalogLoader_Widget::_onBrowseButtonClicked);
    connect(ui->load_button, &QPushButton::clicked, 
            this, &CSVAnalogLoader_Widget::_onLoadButtonClicked);
    connect(ui->single_column_checkbox, &QCheckBox::toggled,
            this, &CSVAnalogLoader_Widget::_onSingleColumnCheckboxToggled);

    // Initialize UI state
    _updateColumnControlsState();
}

CSVAnalogLoader_Widget::~CSVAnalogLoader_Widget()
{
    delete ui;
}

void CSVAnalogLoader_Widget::_onBrowseButtonClicked()
{
    QString selectedPath = QFileDialog::getOpenFileName(
        this,
        "Select CSV File",
        "",
        "CSV Files (*.csv);;All Files (*)"
    );
    
    if (!selectedPath.isEmpty()) {
        ui->file_path_edit->setText(selectedPath);
    }
}

void CSVAnalogLoader_Widget::_onLoadButtonClicked()
{
    QString filePath = ui->file_path_edit->text().trimmed();
    
    if (filePath.isEmpty()) {
        QMessageBox::warning(this, "No File Selected", "Please select a CSV file to load.");
        return;
    }
    
    CSVAnalogLoaderOptions options;
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
    
    options.has_header = ui->has_header_checkbox->isChecked();
    options.single_column_format = ui->single_column_checkbox->isChecked();
    
    if (!options.single_column_format) {
        options.time_column = ui->time_column_spinbox->value();
        options.data_column = ui->data_column_spinbox->value();
        
        // Validate column indices
        if (options.time_column == options.data_column) {
            QMessageBox::warning(this, "Invalid Column Configuration", 
                                "Time and data columns cannot be the same. Please select different column indices.");
            return;
        }
    }
    
    emit loadAnalogCSVRequested(options);
}

void CSVAnalogLoader_Widget::_onSingleColumnCheckboxToggled(bool checked)
{
    Q_UNUSED(checked)
    _updateColumnControlsState();
}

void CSVAnalogLoader_Widget::_updateColumnControlsState()
{
    bool isSingleColumn = ui->single_column_checkbox->isChecked();
    
    // Column controls are only relevant for two-column format
    ui->label_time_column->setEnabled(!isSingleColumn);
    ui->time_column_spinbox->setEnabled(!isSingleColumn);
    ui->label_data_column->setEnabled(!isSingleColumn);
    ui->data_column_spinbox->setEnabled(!isSingleColumn);
} 