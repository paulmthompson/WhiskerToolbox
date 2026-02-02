#include "CSVAnalogSaver_Widget.hpp"
#include "ui_CSVAnalogSaver_Widget.h"

#include "DataManager/IO/formats/CSV/analogtimeseries/Analog_Time_Series_CSV.hpp"// For CSVAnalogSaverOptions

#include <QCheckBox>
#include <QComboBox>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QSpinBox>
#include <QString>

#include <iomanip>// For std::fixed and std::setprecision
#include <sstream>// For std::ostringstream

CSVAnalogSaver_Widget::CSVAnalogSaver_Widget(QWidget * parent)
    : QWidget(parent),
      ui(new Ui::CSVAnalogSaver_Widget) {
    ui->setupUi(this);
    connect(ui->save_action_button, &QPushButton::clicked, this, [this]() {
        CSVAnalogSaverOptions options;
        // Filename and parent_dir are not set here; they are set by the calling widget.

        // Delimiter
        QString delimiterText = ui->delimiter_combo->currentText();
        if (delimiterText == "Comma") {
            options.delimiter = ",";
        } else if (delimiterText == "Space") {
            options.delimiter = " ";
        } else if (delimiterText == "Tab") {
            options.delimiter = "\t";
        } else {
            options.delimiter = ",";// Default
        }

        // Line Ending
        QString lineEndingText = ui->line_ending_combo->currentText();
        if (lineEndingText == "LF (\n)") {
            options.line_delim = "\n";
        } else if (lineEndingText == "CRLF (\r\n)") {
            options.line_delim = "\r\n";
        } else {
            options.line_delim = "\n";// Default
        }

        options.precision = ui->precision_spinbox->value();

        options.save_header = ui->save_header_checkbox->isChecked();
        if (options.save_header) {
            options.header = ui->header_text_edit->text().toStdString();
        } else {
            options.header = "";// Clear header if not saving
        }

        emit saveAnalogCSVRequested(options);
    });

    connect(ui->save_header_checkbox, &QCheckBox::toggled, this, &CSVAnalogSaver_Widget::_onSaveHeaderCheckboxToggled);
    connect(ui->precision_spinbox, qOverload<int>(&QSpinBox::valueChanged), this, &CSVAnalogSaver_Widget::_updatePrecisionExample);

    // Initialize header text edit state based on checkbox
    _onSaveHeaderCheckboxToggled(ui->save_header_checkbox->isChecked());
    // Initialize precision example text based on default precision
    _updatePrecisionLabelText(ui->precision_spinbox->value());
}

CSVAnalogSaver_Widget::~CSVAnalogSaver_Widget() {
    delete ui;
}

void CSVAnalogSaver_Widget::_onSaveHeaderCheckboxToggled(bool checked) {
    ui->header_text_edit->setEnabled(checked);
}

void CSVAnalogSaver_Widget::_updatePrecisionExample(int precision) {
    _updatePrecisionLabelText(precision);
}

void CSVAnalogSaver_Widget::_updatePrecisionLabelText(int precision) {
    std::ostringstream oss;
    oss << std::fixed << std::setprecision(precision) << 1.23456789;// Example number
    QString exampleText = "e.g., " + QString::fromStdString(oss.str());
    ui->label_precision_example->setText(exampleText);
}