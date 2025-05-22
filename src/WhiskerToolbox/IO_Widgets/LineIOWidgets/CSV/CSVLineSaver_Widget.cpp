#include "CSVLineSaver_Widget.hpp"
#include "ui_CSVLineSaver_Widget.h"

#include <QCheckBox>
#include <QComboBox>
#include <QLineEdit>
#include <QSpinBox>
#include <QLabel>
#include <QString>
#include <iomanip> // For std::fixed and std::setprecision
#include <sstream>   // For std::ostringstream

CSVLineSaver_Widget::CSVLineSaver_Widget(QWidget *parent) :
    QWidget(parent),
    ui(new Ui::CSVLineSaver_Widget)
{
    ui->setupUi(this);
    connect(ui->save_action_button, &QPushButton::clicked, this, [this]() {
        CSVSingleFileLineSaverOptions options;
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

        options.precision = ui->precision_spinbox->value();

        options.save_header = ui->save_header_checkbox->isChecked();
        if (options.save_header) {
            options.header = ui->header_text_edit->text().toStdString();
        } else {
            options.header = ""; // Clear header if not saving
        }

        emit saveCSVRequested(options);
    });

    connect(ui->save_header_checkbox, &QCheckBox::toggled, this, &CSVLineSaver_Widget::_onSaveHeaderCheckboxToggled);
    connect(ui->precision_spinbox, qOverload<int>(&QSpinBox::valueChanged), this, &CSVLineSaver_Widget::_updatePrecisionExample);

    // Initialize header text edit state based on checkbox
    _onSaveHeaderCheckboxToggled(ui->save_header_checkbox->isChecked());
    // Initialize precision example text
    _updatePrecisionLabelText(ui->precision_spinbox->value());
}

CSVLineSaver_Widget::~CSVLineSaver_Widget()
{
    delete ui;
}

void CSVLineSaver_Widget::_onSaveHeaderCheckboxToggled(bool checked)
{
    ui->header_text_edit->setEnabled(checked);
}

void CSVLineSaver_Widget::_updatePrecisionExample(int precision)
{
    _updatePrecisionLabelText(precision);
}

void CSVLineSaver_Widget::_updatePrecisionLabelText(int precision)
{
    std::ostringstream oss;
    oss << std::fixed << std::setprecision(precision) << 1.23456789;
    QString exampleText = "e.g., " + QString::fromStdString(oss.str());
    ui->label_precision_example->setText(exampleText);
} 