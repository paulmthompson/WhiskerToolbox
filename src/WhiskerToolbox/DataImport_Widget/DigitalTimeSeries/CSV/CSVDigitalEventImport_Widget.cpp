#include "CSVDigitalEventImport_Widget.hpp"
#include "ui_CSVDigitalEventImport_Widget.h"

#include <QPushButton>
#include <QFileDialog>
#include <QLineEdit>
#include <QComboBox>
#include <QSpinBox>
#include <QCheckBox>
#include <QLabel>
#include <QMessageBox>

CSVDigitalEventImport_Widget::CSVDigitalEventImport_Widget(QWidget * parent)
    : QWidget(parent),
      ui(new Ui::CSVDigitalEventImport_Widget) {
    ui->setupUi(this);
    
    connect(ui->browse_button, &QPushButton::clicked, 
            this, &CSVDigitalEventImport_Widget::_onBrowseButtonClicked);
    connect(ui->load_button, &QPushButton::clicked, 
            this, &CSVDigitalEventImport_Widget::_onLoadButtonClicked);
    connect(ui->has_identifier_checkbox, &QCheckBox::toggled, 
            this, &CSVDigitalEventImport_Widget::_onIdentifierCheckboxToggled);
    
    _updateUIForIdentifierMode();
}

CSVDigitalEventImport_Widget::~CSVDigitalEventImport_Widget() {
    delete ui;
}

void CSVDigitalEventImport_Widget::_onBrowseButtonClicked() {
    QString selectedPath = QFileDialog::getOpenFileName(
        this,
        tr("Select CSV File"),
        QString(),
        tr("CSV Files (*.csv);;All Files (*)"));
    
    if (!selectedPath.isEmpty()) {
        ui->file_path_edit->setText(selectedPath);
    }
}

void CSVDigitalEventImport_Widget::_onLoadButtonClicked() {
    QString filePath = ui->file_path_edit->text().trimmed();
    
    if (filePath.isEmpty()) {
        QMessageBox::warning(this, tr("No File Selected"), 
                            tr("Please select a CSV file to load."));
        return;
    }
    
    CSVEventLoaderOptions options;
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
    options.event_column = ui->event_column_spinbox->value();
    
    if (ui->has_identifier_checkbox->isChecked()) {
        options.identifier_column = ui->identifier_column_spinbox->value();
        
        if (options.event_column == options.identifier_column) {
            QMessageBox::warning(this, tr("Invalid Column Configuration"), 
                tr("Event and Identifier columns cannot be the same. Please select different column indices."));
            return;
        }
    } else {
        options.identifier_column = -1;
    }
    
    emit loadCSVEventRequested(options);
}

void CSVDigitalEventImport_Widget::_onIdentifierCheckboxToggled(bool checked) {
    Q_UNUSED(checked)
    _updateUIForIdentifierMode();
}

void CSVDigitalEventImport_Widget::_updateUIForIdentifierMode() {
    bool has_identifier = ui->has_identifier_checkbox->isChecked();
    ui->label_identifier_column->setEnabled(has_identifier);
    ui->identifier_column_spinbox->setEnabled(has_identifier);
}
