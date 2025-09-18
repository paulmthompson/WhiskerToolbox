#include "TableJSONWidget.hpp"
#include "ui_TableJSONWidget.h"

#include <QFile>
#include <QFileDialog>
#include <QMessageBox>
#include <QTextStream>

TableJSONWidget::TableJSONWidget(QWidget * parent)
    : QWidget(parent),
      ui(new Ui::TableJSONWidget) {
    ui->setupUi(this);
    connect(ui->load_json_btn, &QPushButton::clicked, this, &TableJSONWidget::onLoadJsonClicked);
    connect(ui->apply_json_btn, &QPushButton::clicked, this, &TableJSONWidget::onUpdateTableClicked);
    connect(ui->save_json_btn, &QPushButton::clicked, this, &TableJSONWidget::onSaveJsonClicked);
}

TableJSONWidget::~TableJSONWidget() { delete ui; }

void TableJSONWidget::setJsonText(QString const & text) {
    if (ui->json_text_edit) ui->json_text_edit->setPlainText(text);
}

QString TableJSONWidget::getJsonText() const {
    return ui->json_text_edit ? ui->json_text_edit->toPlainText() : QString();
}

void TableJSONWidget::onLoadJsonClicked() {
    QString filename = m_forcedLoadPath;
    if (filename.isEmpty()) {
        filename = QFileDialog::getOpenFileName(this, "Load Table JSON", QString(), "JSON Files (*.json);;All Files (*)");
    }
    if (filename.isEmpty()) return;
    QFile file(filename);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        QMessageBox::warning(this, "Error", QString("Could not open file: %1").arg(filename));
        return;
    }
    QTextStream in(&file);
    QString text = in.readAll();
    file.close();
    setJsonText(text);
}

void TableJSONWidget::onUpdateTableClicked() {
    emit updateRequested(getJsonText());
}

void TableJSONWidget::setForcedLoadPathForTests(QString const & path) {
    m_forcedLoadPath = path;
}

void TableJSONWidget::onSaveJsonClicked() {
    QString filename = QFileDialog::getSaveFileName(this, "Save Table JSON", QString(), "JSON Files (*.json);;All Files (*)");
    if (filename.isEmpty()) return;
    if (!filename.endsWith(".json", Qt::CaseInsensitive)) filename += ".json";
    QFile file(filename);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        QMessageBox::warning(this, "Error", QString("Could not open file for writing: %1").arg(filename));
        return;
    }
    QTextStream out(&file);
    out << getJsonText();
    file.close();
}
