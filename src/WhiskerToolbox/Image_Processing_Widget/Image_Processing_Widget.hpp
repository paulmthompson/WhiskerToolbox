#ifndef IMAGE_PROCESSING_WIDGET_HPP
#define IMAGE_PROCESSING_WIDGET_HPP

#include <QMainWindow>

#include "DataManager/DataManager.hpp"

#include <opencv2/imgcodecs.hpp>


namespace Ui {
class Image_Processing_Widget;
}

class Image_Processing_Widget : public QMainWindow
{
    Q_OBJECT
public:

    Image_Processing_Widget(std::shared_ptr<DataManager> data_manager, QWidget *parent = 0);
    void openWidget();

void hello();

private:
    Ui::Image_Processing_Widget *ui;
    std::shared_ptr<DataManager> _data_manager;
    double _alpha = 1;
    int _beta = 0;

    void _updateFilters();


private slots:
    void _updAlpha();
    void _updBeta();
};

#endif // IMAGE_PROCESSING_WIDGET_HPP
