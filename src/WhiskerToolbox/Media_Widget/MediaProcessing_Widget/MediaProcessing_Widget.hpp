#ifndef MEDIAPROCESSING_WIDGET_HPP
#define MEDIAPROCESSING_WIDGET_HPP

#include "DataManager/utils/ProcessingOptions.hpp"

#include <QWidget>

#include <memory>
#include <string>

namespace Ui {
class MediaProcessing_Widget;
}

class DataManager;
class Media_Window;
class ContrastWidget;
class GammaWidget;
class SharpenWidget;
class BilateralWidget;
class Section;

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

    // Processing widgets
    ContrastWidget* _contrast_widget;
    Section* _contrast_section;
    GammaWidget* _gamma_widget;
    Section* _gamma_section;
    SharpenWidget* _sharpen_widget;
    Section* _sharpen_section;
    BilateralWidget* _bilateral_widget;
    Section* _bilateral_section;

    // Current processing options
    ContrastOptions _contrast_options;
    GammaOptions _gamma_options;
    SharpenOptions _sharpen_options;
    BilateralOptions _bilateral_options;

    // Legacy parameters for other filters (to be refactored later)
    int _clahe_grid = 8;
    double _clahe_clip = 2.0;
    bool _clahe_active{false};

    void _setupProcessingWidgets();
    void _applyContrastFilter();
    void _applyGammaFilter();
    void _applySharpenFilter();
    void _applyBilateralFilter();
    void _updateClaheFilter();

private slots:
    void _onContrastOptionsChanged(ContrastOptions const& options);
    void _onGammaOptionsChanged(GammaOptions const& options);
    void _onSharpenOptionsChanged(SharpenOptions const& options);
    void _onBilateralOptionsChanged(BilateralOptions const& options);

    // Legacy slots for other filters (to be refactored later)
    void _updateClaheGrid();
    void _updateClaheClip();
    void _activateClahe();
};

#endif // MEDIAPROCESSING_WIDGET_HPP 