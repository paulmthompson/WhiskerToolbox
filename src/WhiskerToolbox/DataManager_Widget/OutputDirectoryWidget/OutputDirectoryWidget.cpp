
#include "OutputDirectoryWidget.hpp"

#include "ui_OutputDirectoryWidget.h"

#include <QFileDialog>

#include <filesystem>

OutputDirectoryWidget::OutputDirectoryWidget(QWidget * parent)
    : QWidget(parent),
      ui(new Ui::OutputDirectoryWidget) {
    ui->setupUi(this);

    ui->output_dir_label->setText(QString::fromStdString(std::filesystem::current_path().string()));

    connect(ui->output_dir_button, &QPushButton::clicked, this, &OutputDirectoryWidget::_changeOutputDir);
}

OutputDirectoryWidget::~OutputDirectoryWidget() {
    delete ui;
}

void OutputDirectoryWidget::setDirLabel(QString const label) {
    ui->output_dir_label->setText(label);
}

void OutputDirectoryWidget::_changeOutputDir() {

    QString const dir_name = QFileDialog::getExistingDirectory(
            this,
            "Select Directory",
            QDir::currentPath());


    emit dirChanged(dir_name);
}
