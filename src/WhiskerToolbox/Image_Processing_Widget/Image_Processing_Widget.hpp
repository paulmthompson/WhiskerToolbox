#ifndef IMAGE_PROCESSING_WIDGET_HPP
#define IMAGE_PROCESSING_WIDGET_HPP

#include <QMainWindow>

#include "DataManager/DataManager.hpp"


namespace Ui {
class Image_Processing_Widget;
}

class Image_Processing_Widget : public QMainWindow
{
    Q_OBJECT
public:

    Image_Processing_Widget(std::shared_ptr<DataManager> data_manager, QWidget *parent = 0);
    void openWidget();

private:
    Ui::Image_Processing_Widget *ui;
    std::shared_ptr<DataManager> _data_manager;
};

#endif // IMAGE_PROCESSING_WIDGET_HPP
