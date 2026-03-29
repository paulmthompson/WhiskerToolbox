/**
 * @file CSVTensorSaver_Widget.cpp
 * @brief Implementation of the CSV tensor saver widget
 */

#include "CSVTensorSaver_Widget.hpp"
#include "ui_CSVTensorSaver_Widget.h"

#include <QCheckBox>
#include <QComboBox>
#include <QPushButton>
#include <QSpinBox>

#include <iomanip>
#include <sstream>

CSVTensorSaver_Widget::CSVTensorSaver_Widget(QWidget * parent)
    : QWidget(parent),
      ui(new Ui::CSVTensorSaver_Widget) {
    ui->setupUi(this);

    connect(ui->save_action_button, &QPushButton::clicked,
            this, &CSVTensorSaver_Widget::_onSaveActionButtonClicked);
    connect(ui->save_header_checkbox, &QCheckBox::toggled,
            this, &CSVTensorSaver_Widget::_onSaveHeaderCheckboxToggled);
    connect(ui->precision_spinbox, qOverload<int>(&QSpinBox::valueChanged),
            this, &CSVTensorSaver_Widget::_updatePrecisionExample);

    _onSaveHeaderCheckboxToggled(ui->save_header_checkbox->isChecked());
    _updatePrecisionLabelText(ui->precision_spinbox->value());
}

CSVTensorSaver_Widget::~CSVTensorSaver_Widget() {
    delete ui;
}

void CSVTensorSaver_Widget::_onSaveActionButtonClicked() {
    emit saveTensorCSVRequested(_getOptionsFromUI());
}

void CSVTensorSaver_Widget::_onSaveHeaderCheckboxToggled(bool checked) {
    Q_UNUSED(checked)
}

void CSVTensorSaver_Widget::_updatePrecisionExample(int precision) {
    _updatePrecisionLabelText(precision);
}

void CSVTensorSaver_Widget::_updatePrecisionLabelText(int precision) {
    std::ostringstream oss;
    oss << std::fixed << std::setprecision(precision) << 1.23456789;
    QString const exampleText = "e.g., " + QString::fromStdString(oss.str());
    ui->label_precision_example->setText(exampleText);
}

CSVTensorSaverOptions CSVTensorSaver_Widget::_getOptionsFromUI() const {
    CSVTensorSaverOptions options;

    QString const delimiterText = ui->delimiter_combo->currentText();
    if (delimiterText == "Comma") {
        options.delimiter = ",";
    } else if (delimiterText == "Space") {
        options.delimiter = " ";
    } else if (delimiterText == "Tab") {
        options.delimiter = "\t";
    }

    options.precision = ui->precision_spinbox->value();
    options.save_header = ui->save_header_checkbox->isChecked();

    return options;
}
