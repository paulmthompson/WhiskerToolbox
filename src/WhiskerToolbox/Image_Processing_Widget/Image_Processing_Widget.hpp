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

    double _contrast_alpha = 1;
    int _contrast_beta = 0;
    double _sharpen_sigma = 3.0;

    int _clahe_grid = 8;
    double _clahe_clip = 2.0;

    void _updateContrastFilter();
    void _updateSharpenFilter();
    void _updateClaheFilter();

private slots:
    void _updateContrastAlpha();
    void _updateContrastBeta();

    void _updateSharpenSigma();

    void _updateClaheGrid();
    void _updateClaheClip();
};

#endif // IMAGE_PROCESSING_WIDGET_HPP
