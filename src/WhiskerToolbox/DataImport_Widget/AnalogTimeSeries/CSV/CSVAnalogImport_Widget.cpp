#include "CSVAnalogImport_Widget.hpp"
#include "ui_CSVAnalogImport_Widget.h"

#include <QPushButton>
#include <QFileDialog>
#include <QLineEdit>
#include <QComboBox>
#include <QSpinBox>
#include <QCheckBox>
#include <QMessageBox>

CSVAnalogImport_Widget::CSVAnalogImport_Widget(QWidget * parent)
    : QWidget(parent),
      ui(new Ui::CSVAnalogImport_Widget) {
    ui->setupUi(this);
    
    connect(ui->browse_button, &QPushButton::clicked, 
            this, &CSVAnalogImport_Widget::_onBrowseButtonClicked);
    connect(ui->load_button, &QPushButton::clicked, 
            this, &CSVAnalogImport_Widget::_onLoadButtonClicked);
    connect(ui->format_combo, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &CSVAnalogImport_Widget::_onFormatChanged);
    
    // Initialize visibility based on default format
    _updateColumnVisibility();
}

CSVAnalogImport_Widget::~CSVAnalogImport_Widget() {
    delete ui;
}

void CSVAnalogImport_Widget::_onBrowseButtonClicked() {
    QString selectedPath = QFileDialog::getOpenFileName(
        this,
        tr("Select CSV File"),
        QString(),
        tr("CSV Files (*.csv);;Text Files (*.txt);;All Files (*)"));
    
    if (!selectedPath.isEmpty()) {
        ui->file_path_edit->setText(selectedPath);
    }
}

void CSVAnalogImport_Widget::_onLoadButtonClicked() {
    QString filePath = ui->file_path_edit->text().trimmed();
    
    if (filePath.isEmpty()) {
        QMessageBox::warning(this, tr("No File Selected"), 
                            tr("Please select a CSV file to load."));
        return;
    }
    
    CSVAnalogLoaderOptions options;
    options.filepath = filePath.toStdString();
    
    QString delimiterText = ui->delimiter_combo->currentText();
    if (delimiterText == "Comma") {
        options.delimiter = ",";
    } else if (delimiterText == "Space") {
        options.delimiter = " ";
    } else if (delimiterText == "Tab") {
        options.delimiter = "\t";
    } else if (delimiterText == "Semicolon") {
        options.delimiter = ";";
    } else {
        options.delimiter = ",";
    }
    
    options.has_header = ui->has_header_checkbox->isChecked();
    
    // Check format selection
    bool isSingleColumn = (ui->format_combo->currentIndex() == 0);
    options.single_column_format = isSingleColumn;
    
    if (!isSingleColumn) {
        options.time_column = ui->time_column_spinbox->value();
        options.data_column = ui->data_column_spinbox->value();
        
        if (options.time_column.value().value() == options.data_column.value().value()) {
            QMessageBox::warning(this, tr("Invalid Column Configuration"), 
                tr("Time and Data columns cannot be the same. Please select different column indices."));
            return;
        }
    }
    
    emit loadAnalogCSVRequested(options);
}

void CSVAnalogImport_Widget::_onFormatChanged(int index) {
    Q_UNUSED(index)
    _updateColumnVisibility();
}

void CSVAnalogImport_Widget::_updateColumnVisibility() {
    bool isTwoColumn = (ui->format_combo->currentIndex() == 1);
    ui->label_time_column->setVisible(isTwoColumn);
    ui->time_column_spinbox->setVisible(isTwoColumn);
    ui->label_data_column->setVisible(isTwoColumn);
    ui->data_column_spinbox->setVisible(isTwoColumn);
}
