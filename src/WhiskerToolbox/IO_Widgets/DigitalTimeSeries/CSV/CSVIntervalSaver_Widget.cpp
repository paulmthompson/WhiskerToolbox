#include "CSVIntervalSaver_Widget.hpp"
#include "ui_CSVIntervalSaver_Widget.h"

#include <QCheckBox>
#include <QComboBox>
#include <QLineEdit>
#include <QPushButton>

CSVIntervalSaver_Widget::CSVIntervalSaver_Widget(QWidget *parent) :
    QWidget(parent),
    ui(new Ui::CSVIntervalSaver_Widget)
{
    ui->setupUi(this);

    // Connect signals and slots
    connect(ui->save_action_button, &QPushButton::clicked, 
            this, &CSVIntervalSaver_Widget::_onSaveActionButtonClicked);
    connect(ui->save_header_checkbox, &QCheckBox::toggled, 
            this, &CSVIntervalSaver_Widget::_onSaveHeaderCheckboxToggled);

    // Initialize header text edit state based on checkbox
    _onSaveHeaderCheckboxToggled(ui->save_header_checkbox->isChecked());
}

CSVIntervalSaver_Widget::~CSVIntervalSaver_Widget()
{
    delete ui;
}

void CSVIntervalSaver_Widget::_onSaveActionButtonClicked()
{
    emit saveIntervalCSVRequested(_getOptionsFromUI());
}

void CSVIntervalSaver_Widget::_onSaveHeaderCheckboxToggled(bool checked)
{
    ui->header_text_edit->setEnabled(checked);
}

CSVIntervalSaverOptions CSVIntervalSaver_Widget::_getOptionsFromUI() const
{
    CSVIntervalSaverOptions options;

    // Delimiter
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
    
    // filename and parent_dir are not set here as per requirements

    return options;
} 