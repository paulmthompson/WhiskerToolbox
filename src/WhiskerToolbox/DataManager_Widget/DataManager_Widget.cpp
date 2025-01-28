
#include "DataManager_Widget.hpp"
#include "ui_DataManager_Widget.h"

#include <QFileDialog>

DataManager_Widget::DataManager_Widget(std::shared_ptr<DataManager> data_manager, QWidget *parent) :
    QWidget(parent),
    ui(new Ui::DataManager_Widget),
    _data_manager{data_manager},
    _output_path{std::filesystem::current_path()}
{
    ui->setupUi(this);

    ui->feature_table_widget->setColumns({"Feature", "Type", "Clock"});
    ui->feature_table_widget->setDataManager(_data_manager);

    ui->output_dir_label->setText(QString::fromStdString(std::filesystem::current_path().string()));

    connect(ui->output_dir_button, &QPushButton::clicked, this, &DataManager_Widget::_changeOutputDir);
    connect(ui->feature_table_widget, &Feature_Table_Widget::featureSelected, this, &DataManager_Widget::_handleFeatureSelected);
}

DataManager_Widget::~DataManager_Widget() {
    delete ui;
}

void DataManager_Widget::openWidget()
{
    ui->feature_table_widget->populateTable();
    this->show();
}

void DataManager_Widget::_handleFeatureSelected(const QString& feature)
{
    _highlighted_available_feature = feature;
}


void DataManager_Widget::_changeOutputDir()
{
    QString dir_name = QFileDialog::getExistingDirectory(
        this,
        "Select Directory",
        QDir::currentPath());

    if (dir_name.isEmpty()) {
        return;
    }

    _output_path = std::filesystem::path(dir_name.toStdString());
    ui->output_dir_label->setText(dir_name);
}
