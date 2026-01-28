#include "CSVDigitalIntervalImport_Widget.hpp"
#include "ui_CSVDigitalIntervalImport_Widget.h"

#include <QPushButton>
#include <QFileDialog>
#include <QLineEdit>
#include <QComboBox>
#include <QSpinBox>
#include <QCheckBox>
#include <QMessageBox>

CSVDigitalIntervalImport_Widget::CSVDigitalIntervalImport_Widget(QWidget * parent)
    : QWidget(parent),
      ui(new Ui::CSVDigitalIntervalImport_Widget) {
    ui->setupUi(this);
    
    connect(ui->browse_button, &QPushButton::clicked, 
            this, &CSVDigitalIntervalImport_Widget::_onBrowseButtonClicked);
    connect(ui->load_button, &QPushButton::clicked, 
            this, &CSVDigitalIntervalImport_Widget::_onLoadButtonClicked);
}

CSVDigitalIntervalImport_Widget::~CSVDigitalIntervalImport_Widget() {
    delete ui;
}

void CSVDigitalIntervalImport_Widget::_onBrowseButtonClicked() {
    QString selectedPath = QFileDialog::getOpenFileName(
        this,
        tr("Select CSV File"),
        QString(),
        tr("CSV Files (*.csv);;All Files (*)"));
    
    if (!selectedPath.isEmpty()) {
        ui->file_path_edit->setText(selectedPath);
    }
}

void CSVDigitalIntervalImport_Widget::_onLoadButtonClicked() {
    QString filePath = ui->file_path_edit->text().trimmed();
    
    if (filePath.isEmpty()) {
        QMessageBox::warning(this, tr("No File Selected"), 
                            tr("Please select a CSV file to load."));
        return;
    }
    
    CSVIntervalLoaderOptions options;
    options.filepath = filePath.toStdString();
    
    QString delimiterText = ui->delimiter_combo->currentText();
    if (delimiterText == "Comma") {
        options.delimiter = ",";
    } else if (delimiterText == "Space") {
        options.delimiter = " ";
    } else if (delimiterText == "Tab") {
        options.delimiter = "\t";
    } else {
        options.delimiter = ",";
    }
    
    options.has_header = ui->has_header_checkbox->isChecked();
    options.start_column = ui->start_column_spinbox->value();
    options.end_column = ui->end_column_spinbox->value();
    
    if (options.start_column == options.end_column) {
        QMessageBox::warning(this, tr("Invalid Column Configuration"), 
            tr("Start and End columns cannot be the same. Please select different column indices."));
        return;
    }
    
    emit loadCSVIntervalRequested(options);
}
