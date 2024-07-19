#ifndef IMAGE_PROCESSING_WIDGET_HPP
#define IMAGE_PROCESSING_WIDGET_HPP

#include <QMainWindow>

class DataManager;
class Media_Window;

namespace Ui {
class Image_Processing_Widget;
}

class Image_Processing_Widget : public QMainWindow
{
    Q_OBJECT
public:

    Image_Processing_Widget(Media_Window* scene, std::shared_ptr<DataManager> data_manager, QWidget *parent = 0);
    void openWidget();

void hello();

private:
    Ui::Image_Processing_Widget *ui;
    std::shared_ptr<DataManager> _data_manager;
    Media_Window * _scene;

    double _contrast_alpha = 1;
    int _contrast_beta = 0;
    bool _contrast_active {false};

    double _sharpen_sigma = 3.0;
    bool _sharpen_active {false};

    int _clahe_grid = 8;
    double _clahe_clip = 2.0;
    bool _clahe_active {false};

    int _bilateral_d = 5;
    double _bilateral_spatial_sigma = 20.0;
    double _bilateral_color_sigma = 20.0;
    bool _bilateral_active {false};

    void _updateContrastFilter();
    void _updateSharpenFilter();
    void _updateClaheFilter();
    void _updateBilateralFilter();

private slots:
    void _updateContrastAlpha();
    void _updateContrastBeta();
    void _activateContrast();

    void _updateSharpenSigma();
    void _activateSharpen();

    void _updateClaheGrid();
    void _updateClaheClip();
    void _activateClahe();

    void _updateBilateralD();
    void _updateBilateralSpatialSigma();
    void _updateBilateralColorSigma();
    void _activateBilateral();
};

#endif // IMAGE_PROCESSING_WIDGET_HPP
