#include "CSVDigitalEventLoader_Widget.hpp"
#include "ui_CSVDigitalEventLoader_Widget.h"

#include <QPushButton>
#include <QFileDialog>
#include <QLineEdit>
#include <QComboBox>
#include <QSpinBox>
#include <QCheckBox>
#include <QLabel>
#include <QMessageBox>

CSVDigitalEventLoader_Widget::CSVDigitalEventLoader_Widget(QWidget *parent) :
    QWidget(parent),
    ui(new Ui::CSVDigitalEventLoader_Widget)
{
    ui->setupUi(this);
    
    // Connect signals
    connect(ui->browse_button, &QPushButton::clicked, this, &CSVDigitalEventLoader_Widget::_onBrowseButtonClicked);
    connect(ui->load_button, &QPushButton::clicked, this, &CSVDigitalEventLoader_Widget::_onLoadButtonClicked);
    connect(ui->has_identifier_checkbox, &QCheckBox::toggled, this, &CSVDigitalEventLoader_Widget::_onIdentifierCheckboxToggled);
    
    // Initialize UI state
    _updateUIForIdentifierMode();
}

CSVDigitalEventLoader_Widget::~CSVDigitalEventLoader_Widget()
{
    delete ui;
}

void CSVDigitalEventLoader_Widget::_onBrowseButtonClicked() {
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

void CSVDigitalEventLoader_Widget::_onLoadButtonClicked() {
    QString filePath = ui->file_path_edit->text().trimmed();
    
    if (filePath.isEmpty()) {
        QMessageBox::warning(this, "No File Selected", "Please select a CSV file to load.");
        return;
    }
    
    CSVEventLoaderOptions options;
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
    options.event_column = ui->event_column_spinbox->value();
    
    // Handle identifier column
    if (ui->has_identifier_checkbox->isChecked()) {
        options.identifier_column = ui->identifier_column_spinbox->value();
        
        // Validate column indices
        if (options.event_column == options.identifier_column) {
            QMessageBox::warning(this, "Invalid Column Configuration", 
                                "Event and Identifier columns cannot be the same. Please select different column indices.");
            return;
        }
    } else {
        options.identifier_column = -1; // No identifier column
    }
    
    emit loadCSVEventRequested(options);
}

void CSVDigitalEventLoader_Widget::_onIdentifierCheckboxToggled(bool checked) {
    Q_UNUSED(checked)
    _updateUIForIdentifierMode();
}

void CSVDigitalEventLoader_Widget::_updateUIForIdentifierMode() {
    bool has_identifier = ui->has_identifier_checkbox->isChecked();
    
    // Enable/disable identifier column controls based on checkbox
    ui->label_identifier_column->setEnabled(has_identifier);
    ui->identifier_column_spinbox->setEnabled(has_identifier);
} 