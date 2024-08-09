
#include "Media_Widget.hpp"

#include "ui_Media_Widget.h"

#include "Main_Window/mainwindow.hpp"
#include "Media_Widget/Media_Widget_Items.hpp"
#include "Media_Window/Media_Window.hpp"


Media_Widget::Media_Widget(QWidget *parent) :
    QWidget(parent),
    ui(new Ui::Media_Widget)
{
    ui->setupUi(this);

    connect(ui->data_viewer_button, &QPushButton::clicked, this, &Media_Widget::_openDataViewer);
}

Media_Widget::~Media_Widget() {
    delete ui;
}

void Media_Widget::updateMedia() {

    ui->graphicsView->setScene(_scene);
    ui->graphicsView->show();

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
