#include "CSVDigitalIntervalLoader_Widget.hpp"
#include "ui_CSVDigitalIntervalLoader_Widget.h"

#include <QPushButton>
#include <QFileDialog>
#include <QLineEdit>
#include <QComboBox>
#include <QSpinBox>
#include <QCheckBox>
#include <QMessageBox>

CSVDigitalIntervalLoader_Widget::CSVDigitalIntervalLoader_Widget(QWidget *parent) :
    QWidget(parent),
    ui(new Ui::CSVDigitalIntervalLoader_Widget)
{
    ui->setupUi(this);
    
    // Connect signals
    connect(ui->browse_button, &QPushButton::clicked, this, &CSVDigitalIntervalLoader_Widget::_onBrowseButtonClicked);
    connect(ui->load_button, &QPushButton::clicked, this, &CSVDigitalIntervalLoader_Widget::_onLoadButtonClicked);
}

CSVDigitalIntervalLoader_Widget::~CSVDigitalIntervalLoader_Widget()
{
    delete ui;
}

void CSVDigitalIntervalLoader_Widget::_onBrowseButtonClicked() {
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

void CSVDigitalIntervalLoader_Widget::_onLoadButtonClicked() {
    QString filePath = ui->file_path_edit->text().trimmed();
    
    if (filePath.isEmpty()) {
        QMessageBox::warning(this, "No File Selected", "Please select a CSV file to load.");
        return;
    }
    
    CSVIntervalLoaderOptions options;
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
    options.start_column = ui->start_column_spinbox->value();
    options.end_column = ui->end_column_spinbox->value();
    
    // Validate column indices
    if (options.start_column == options.end_column) {
        QMessageBox::warning(this, "Invalid Column Configuration", 
                            "Start and End columns cannot be the same. Please select different column indices.");
        return;
    }
    
    emit loadCSVIntervalRequested(options);
} 