#include "CSVPointSaver_Widget.hpp"
#include "ui_CSVPointSaver_Widget.h"

// Added includes for QCheckBox, QComboBox, QLineEdit if their methods are directly used beyond ui setup.
// ui_CSVPointSaver_Widget.h should bring them in, but explicit includes are safer for direct method calls.
#include <QCheckBox>
#include <QComboBox>
#include <QLineEdit>

CSVPointSaver_Widget::CSVPointSaver_Widget(QWidget *parent) :
    QWidget(parent),
    ui(new Ui::CSVPointSaver_Widget)
{
    ui->setupUi(this);
    connect(ui->save_action_button, &QPushButton::clicked, this, [this]() {
        CSVPointSaverOptions options;
        options.filename = ui->save_filename_edit->text().toStdString();
        
        // Delimiter
        QString delimiterText = ui->delimiter_saver_combo->currentText();
        if (delimiterText == "Comma") {
            options.delimiter = ",";
        } else if (delimiterText == "Space") {
            options.delimiter = " ";
        } else if (delimiterText == "Tab") {
            options.delimiter = "\t";
        } else {
            options.delimiter = ","; // Default
        }

        // Line Ending
        QString lineEndingText = ui->line_ending_combo->currentText();
        if (lineEndingText == "LF (\n)") {
            options.line_delim = "\n";
        } else if (lineEndingText == "CRLF (\r\n)") {
            options.line_delim = "\r\n";
        } else {
            options.line_delim = "\n"; // Default
        }

        options.save_header = ui->save_header_checkbox->isChecked();
        if (options.save_header) {
            options.header = ui->header_text_edit->text().toStdString();
        } else {
            options.header = ""; // Clear header if not saving
        }

        emit saveCSVRequested(options);
    });

    connect(ui->save_header_checkbox, &QCheckBox::toggled, this, &CSVPointSaver_Widget::_onSaveHeaderCheckboxToggled);
    // Initialize header text edit state based on checkbox
    _onSaveHeaderCheckboxToggled(ui->save_header_checkbox->isChecked());
}

CSVPointSaver_Widget::~CSVPointSaver_Widget()
{
    delete ui;
}

void CSVPointSaver_Widget::_onSaveHeaderCheckboxToggled(bool checked)
{
    ui->header_text_edit->setEnabled(checked);
} 