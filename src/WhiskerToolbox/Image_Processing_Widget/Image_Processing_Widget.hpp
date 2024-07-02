#ifndef IMAGE_PROCESSING_WIDGET_HPP
#define IMAGE_PROCESSING_WIDGET_HPP

#include "DataManager/DataManager.hpp"
#include "Media_Window.hpp"

#include <QMainWindow>

namespace Ui {
class Image_Processing_Widget;
}

class Image_Processing_Widget : public QMainWindow
{
    Q_OBJECT
public:

    Image_Processing_Widget(Media_Window* scene, std::shared_ptr<DataManager> data_manager, QWidget *parent = 0);
    void openWidget();

private:
    Ui::Image_Processing_Widget *ui;
    std::shared_ptr<DataManager> _data_manager;
    Media_Window * _scene;

    void _updateFilters();

    double _alpha = 1;
    int _beta = 0;

private slots:
    void _updAlpha();
    void _updBeta();
};

#endif // IMAGE_PROCESSING_WIDGET_HPP
