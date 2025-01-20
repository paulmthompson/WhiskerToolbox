
#include "Media_Widget.hpp"

#include "ui_Media_Widget.h"

#include "Main_Window/mainwindow.hpp"
#include "Media_Widget/Media_Widget_Items.hpp"
#include "Media_Window/Media_Window.hpp"

#include <QSlider>


Media_Widget::Media_Widget(QWidget *parent) :
    QWidget(parent),
    ui(new Ui::Media_Widget)
{
    ui->setupUi(this);

    connect(ui->data_viewer_button, &QPushButton::clicked, this, &Media_Widget::_openDataViewer);
    connect(ui->mask_slider, &QSlider::valueChanged, this, &Media_Widget::_setMaskAlpha);

    connect(ui->feature_table_widget, &Feature_Table_Widget::addFeature, this, &Media_Widget::_addFeatureToDisplay);

}

Media_Widget::~Media_Widget() {
    delete ui;
}

void Media_Widget::updateMedia() {

    ui->graphicsView->setScene(_scene);
    ui->graphicsView->show();

}

void Media_Widget::setDataManager(std::shared_ptr<DataManager> data_manager)
{
    _data_manager = data_manager;

    ui->feature_table_widget->setColumns({"Feature", "Color", "Enabled", "Type"});
    ui->feature_table_widget->setDataManager(_data_manager);
    ui->feature_table_widget->populateTable();
}

void Media_Widget::_openDataViewer()
{
    std::string const key = "media_widget_item_list";

    auto* media_widget_item_list = _main_window->getWidget(key);
    if (!media_widget_item_list) {
        auto media_widget_item_list_ptr = std::make_unique<Media_Widget_Items>(_data_manager, _scene);
        _main_window->addWidget(key, std::move(media_widget_item_list_ptr));

        media_widget_item_list = _main_window->getWidget(key);
        _main_window->registerDockWidget(key, media_widget_item_list, ads::RightDockWidgetArea);
    }

    _main_window->showDockWidget(key);
}

void Media_Widget::resizeEvent(QResizeEvent* event) {
    QWidget::resizeEvent(event);
    _updateCanvasSize();
}

void Media_Widget::_updateCanvasSize() {
    if (_scene) {
        _scene->setCanvasSize(
                ui->graphicsView->width(),
                ui->graphicsView->height());
        _scene->UpdateCanvas();
    }
}

void Media_Widget::_setMaskAlpha(int alpha)
{
    float alpha_float = static_cast<float>(alpha) / 100;
    _scene->changeMaskAlpha(alpha_float);
}

void Media_Widget::_addFeatureToDisplay()
{
    /*
    auto features = ui->feature_table_widget->getSelectedFeatures();
    for (const auto& feature : features) {
        _scene->addFeatureToDisplay(feature);
    }
    */
}
