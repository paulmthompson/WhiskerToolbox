#include "TableTransformWidget.hpp"
#include "ui_TableTransformWidget.h"

#include <QStringList>

TableTransformWidget::TableTransformWidget(QWidget * parent)
    : QWidget(parent)
    , ui(new Ui::TableTransformWidget)
{
    ui->setupUi(this);

    // Connect the apply transform button to our slot
    connect(ui->apply_transform_btn, &QPushButton::clicked,
            this, &TableTransformWidget::onApplyTransformClicked);
}

TableTransformWidget::~TableTransformWidget() {
    delete ui;
}

QString TableTransformWidget::getTransformType() const {
    return ui->transform_type_combo ? ui->transform_type_combo->currentText() : QString();
}

bool TableTransformWidget::isCenterEnabled() const {
    return ui->transform_center_checkbox && ui->transform_center_checkbox->isChecked();
}

bool TableTransformWidget::isStandardizeEnabled() const {
    return ui->transform_standardize_checkbox && ui->transform_standardize_checkbox->isChecked();
}

std::vector<std::string> TableTransformWidget::getIncludeColumns() const {
    if (!ui->transform_include_edit) {
        return {};
    }
    return parseCommaSeparatedList(ui->transform_include_edit->text());
}

std::vector<std::string> TableTransformWidget::getExcludeColumns() const {
    if (!ui->transform_exclude_edit) {
        return {};
    }
    return parseCommaSeparatedList(ui->transform_exclude_edit->text());
}

QString TableTransformWidget::getOutputName() const {
    return ui->transform_output_name_edit ? ui->transform_output_name_edit->text().trimmed() : QString();
}

void TableTransformWidget::setOutputName(const QString& name) {
    if (ui->transform_output_name_edit) {
        ui->transform_output_name_edit->setText(name);
    }
}

void TableTransformWidget::onApplyTransformClicked() {
    emit applyTransformClicked();
}

std::vector<std::string> TableTransformWidget::parseCommaSeparatedList(const QString& text) const {
    std::vector<std::string> out;
    for (QString s : text.split(",", Qt::SkipEmptyParts)) {
        s = s.trimmed();
        if (!s.isEmpty()) {
            out.push_back(s.toStdString());
        }
    }
    return out;
}
