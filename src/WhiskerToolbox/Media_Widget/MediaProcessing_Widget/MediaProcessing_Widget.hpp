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
class ClaheWidget;
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
    ClaheWidget* _clahe_widget;
    Section* _clahe_section;
    BilateralWidget* _bilateral_widget;
    Section* _bilateral_section;

    // Current processing options
    ContrastOptions _contrast_options;
    GammaOptions _gamma_options;
    SharpenOptions _sharpen_options;
    ClaheOptions _clahe_options;
    BilateralOptions _bilateral_options;

    void _setupProcessingWidgets();
    void _applyContrastFilter();
    void _applyGammaFilter();
    void _applySharpenFilter();
    void _applyClaheFilter();
    void _applyBilateralFilter();

private slots:
    void _onContrastOptionsChanged(ContrastOptions const& options);
    void _onGammaOptionsChanged(GammaOptions const& options);
    void _onSharpenOptionsChanged(SharpenOptions const& options);
    void _onClaheOptionsChanged(ClaheOptions const& options);
    void _onBilateralOptionsChanged(BilateralOptions const& options);
};

#endif // MEDIAPROCESSING_WIDGET_HPP 