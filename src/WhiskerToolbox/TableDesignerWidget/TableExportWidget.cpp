#include "TableExportWidget.hpp"
#include "ui_TableExportWidget.h"

#include <QPushButton>

TableExportWidget::TableExportWidget(QWidget * parent)
    : QWidget(parent), ui(new Ui::TableExportWidget) {
    ui->setupUi(this);
    connect(ui->export_csv_btn, &QPushButton::clicked, this, &TableExportWidget::exportClicked);
}

TableExportWidget::~TableExportWidget() { delete ui; }

QString TableExportWidget::getDelimiterText() const {
    return ui->export_delimiter_combo ? ui->export_delimiter_combo->currentText() : QString("Comma");
}

QString TableExportWidget::getLineEndingText() const {
    return ui->export_line_ending_combo ? ui->export_line_ending_combo->currentText() : QString("LF (\\n)");
}

int TableExportWidget::getPrecision() const {
    return ui->export_precision_spinbox ? ui->export_precision_spinbox->value() : 3;
}

bool TableExportWidget::isHeaderIncluded() const {
    return ui->export_header_checkbox && ui->export_header_checkbox->isChecked();
}


