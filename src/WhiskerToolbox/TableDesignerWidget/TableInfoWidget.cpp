#include "TableInfoWidget.hpp"
#include "ui_TableInfoWidget.h"

TableInfoWidget::TableInfoWidget(QWidget * parent)
    : QWidget(parent),
      ui(new Ui::TableInfoWidget) {
    ui->setupUi(this);
    connect(ui->save_info_btn, &QPushButton::clicked, this, &TableInfoWidget::saveClicked);
}

TableInfoWidget::~TableInfoWidget() { delete ui; }

QString TableInfoWidget::getName() const { return ui->table_name_edit->text().trimmed(); }
QString TableInfoWidget::getDescription() const { return ui->table_description_edit->toPlainText().trimmed(); }
void TableInfoWidget::setName(QString const & name) { ui->table_name_edit->setText(name); }
void TableInfoWidget::setDescription(QString const & desc) { ui->table_description_edit->setPlainText(desc); }
