#ifndef MEDIAPROCESSING_WIDGET_HPP
#define MEDIAPROCESSING_WIDGET_HPP

/**
 * @file MediaProcessing_Widget.hpp
 * @brief Generic media processing widget driven by ProcessingStepRegistry and AutoParamWidget
 *
 * Replaces the former 8 hand-written processing option widgets with auto-generated
 * forms. Standard image filters (Linear Transform, Gamma, Sharpen, CLAHE, Bilateral,
 * Median) are driven entirely by the ProcessingStepRegistry. Colormap and Magic Eraser
 * receive special handling due to rendering-level integration and canvas interaction.
 */

#include "ImageProcessing/ProcessingOptions.hpp"

#include <QWidget>

#include <map>
#include <memory>
#include <string>

namespace Ui {
class MediaProcessing_Widget;
}

class AutoParamWidget;
class DataManager;
class MagicEraserWidget;
class Media_Window;
class MediaWidgetState;
class QCheckBox;
class Section;

class MediaProcessing_Widget : public QWidget {
    Q_OBJECT
public:
    explicit MediaProcessing_Widget(std::shared_ptr<DataManager> data_manager, Media_Window * scene, MediaWidgetState * state, QWidget * parent = nullptr);
    ~MediaProcessing_Widget() override;

    void setActiveKey(std::string const & key);

protected:
    void hideEvent(QHideEvent * event) override;

private:
    Ui::MediaProcessing_Widget * ui;
    std::shared_ptr<DataManager> _data_manager;
    Media_Window * _scene;
    MediaWidgetState * _state;
    std::string _active_key;

    // --- Generic registry-driven processing sections ---
    struct ProcessingSection {
        std::string chain_key;
        Section * section = nullptr;
        QCheckBox * active_checkbox = nullptr;
        AutoParamWidget * param_widget = nullptr;
    };
    std::vector<ProcessingSection> _processing_sections;

    void _setupRegistrySections();
    void _applyRegistryStep(ProcessingSection const & ps);
    void _onRegistryParamsChanged(ProcessingSection const & ps);
    void _onRegistryActiveChanged(ProcessingSection const & ps, bool active);

    // --- Contrast post-edit hook (Phase 5: bidirectional alpha/beta ↔ min/max) ---
    static std::string _contrastPostEditHook(std::string const & json);

    // --- Magic Eraser (special: canvas interaction) ---
    MagicEraserWidget * _magic_eraser_drawing_widget = nullptr;
    AutoParamWidget * _magic_eraser_param_widget = nullptr;
    QCheckBox * _magic_eraser_active_checkbox = nullptr;
    Section * _magic_eraser_section = nullptr;

    void _setupMagicEraserSection();
    void _applyMagicEraser(bool active);
    void _onMagicEraserParamsChanged();
    void _onMagicEraserActiveChanged(bool active);
    void _onMagicEraserDrawingModeChanged(bool enabled);
    void _onMagicEraserClearMaskRequested();

    // --- Colormap (special: rendering-level, not processing chain) ---
    AutoParamWidget * _colormap_param_widget = nullptr;
    QCheckBox * _colormap_active_checkbox = nullptr;
    Section * _colormap_section = nullptr;

    void _setupColormapSection();
    void _onColormapParamsChanged();
    void _onColormapActiveChanged(bool active);
    void _updateColormapAvailability();

    // --- Common ---
    void _loadProcessingChainFromMedia();
    void _updateMedianKernelConstraints();

private slots:
    void _onDrawingFinished();
};

#endif// MEDIAPROCESSING_WIDGET_HPP
