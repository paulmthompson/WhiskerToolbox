#ifndef MEDIAPROCESSING_WIDGET_HPP
#define MEDIAPROCESSING_WIDGET_HPP

#include "ImageProcessing/ProcessingOptions.hpp"

#include <QWidget>

#include <map>
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
class ClaheWidget;
class BilateralWidget;
class MedianWidget;
class MagicEraserWidget;
class ColormapWidget;
class Section;

class MediaProcessing_Widget : public QWidget {
    Q_OBJECT
public:
    explicit MediaProcessing_Widget(std::shared_ptr<DataManager> data_manager, Media_Window * scene, QWidget * parent = nullptr);
    ~MediaProcessing_Widget() override;

    void setActiveKey(std::string const & key);

protected:
    void hideEvent(QHideEvent * event) override;

private:
    Ui::MediaProcessing_Widget * ui;
    std::shared_ptr<DataManager> _data_manager;
    Media_Window * _scene;
    std::string _active_key;

    // Processing widgets
    ContrastWidget * _contrast_widget;
    Section * _contrast_section;
    GammaWidget * _gamma_widget;
    Section * _gamma_section;
    SharpenWidget * _sharpen_widget;
    Section * _sharpen_section;
    ClaheWidget * _clahe_widget;
    Section * _clahe_section;
    BilateralWidget * _bilateral_widget;
    Section * _bilateral_section;
    MedianWidget * _median_widget;
    Section * _median_section;
    MagicEraserWidget * _magic_eraser_widget;
    Section * _magic_eraser_section;
    ColormapWidget * _colormap_widget;
    Section * _colormap_section;

    void _setupProcessingWidgets();
    void _applyContrastFilter(ContrastOptions const & options);
    void _applyGammaFilter(GammaOptions const & options);
    void _applySharpenFilter(SharpenOptions const & options);
    void _applyClaheFilter(ClaheOptions const & options);
    void _applyBilateralFilter(BilateralOptions const & options);
    void _applyMedianFilter(MedianOptions const & options);
    void _applyMagicEraser(MagicEraserOptions const & options);

    void _loadProcessingChainFromMedia();

    void _updateColormapAvailability();

private slots:
    void _onContrastOptionsChanged(ContrastOptions const & options);
    void _onGammaOptionsChanged(GammaOptions const & options);
    void _onSharpenOptionsChanged(SharpenOptions const & options);
    void _onClaheOptionsChanged(ClaheOptions const & options);
    void _onBilateralOptionsChanged(BilateralOptions const & options);
    void _onMedianOptionsChanged(MedianOptions const & options);
    void _onMagicEraserOptionsChanged(MagicEraserOptions const & options);
    void _onMagicEraserDrawingModeChanged(bool enabled);
    void _onDrawingFinished();
    void _onMagicEraserClearMaskRequested();
    void _onColormapOptionsChanged(ColormapOptions const & options);
};

#endif// MEDIAPROCESSING_WIDGET_HPP