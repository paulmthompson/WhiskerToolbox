#ifndef MEDIAPROCESSING_WIDGET_HPP
#define MEDIAPROCESSING_WIDGET_HPP

#include <QWidget>

#include <memory>
#include <string>

namespace Ui {
class MediaProcessing_Widget;
}

class DataManager;
class Media_Window;

class MediaProcessing_Widget : public QWidget {
    Q_OBJECT
public:
    explicit MediaProcessing_Widget(std::shared_ptr<DataManager> data_manager, Media_Window* scene, QWidget* parent = nullptr);
    ~MediaProcessing_Widget() override;

    void setActiveKey(std::string const& key);

private:
    Ui::MediaProcessing_Widget* ui;
    std::shared_ptr<DataManager> _data_manager;
    Media_Window* _scene;
    std::string _active_key;

    // Image processing parameters
    double _contrast_alpha = 1.0;
    int _contrast_beta = 0;
    bool _contrast_active{false};

    double _gamma = 1.0;
    bool _gamma_active{false};

    double _sharpen_sigma = 3.0;
    bool _sharpen_active{false};

    int _clahe_grid = 8;
    double _clahe_clip = 2.0;
    bool _clahe_active{false};

    int _bilateral_d = 5;
    double _bilateral_spatial_sigma = 20.0;
    double _bilateral_color_sigma = 20.0;
    bool _bilateral_active{false};

    void _updateContrastFilter();
    void _updateGammaFilter();
    void _updateSharpenFilter();
    void _updateClaheFilter();
    void _updateBilateralFilter();

private slots:
    void _updateContrastAlpha();
    void _updateContrastBeta();
    void _activateContrast();

    void _updateGamma();
    void _activateGamma();

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

#endif // MEDIAPROCESSING_WIDGET_HPP 