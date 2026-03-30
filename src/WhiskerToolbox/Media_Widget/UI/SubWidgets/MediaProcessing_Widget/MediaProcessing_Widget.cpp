/**
 * @file MediaProcessing_Widget.cpp
 * @brief Generic media processing widget using ProcessingStepRegistry and AutoParamWidget
 */

#include "MediaProcessing_Widget.hpp"
#include "ui_MediaProcessing_Widget.h"

#include "Media_Widget/Core/MediaWidgetState.hpp"
#include "Media_Widget/Rendering/Media_Window/Media_Window.hpp"
#include "ProcessingOptions/MagicEraserWidget.hpp"

#include "AutoParamWidget/AutoParamWidget.hpp"
#include "Collapsible_Widget/Section.hpp"
#include "DataManager/DataManager.hpp"
#include "ImageProcessing/OpenCVUtility.hpp"
#include "Media/Media_Data.hpp"
#include "MediaProcessingPipeline/ProcessingOptionsSchema.hpp"
#include "MediaProcessingPipeline/ProcessingStepRegistry.hpp"

#include <rfl/json.hpp>

#include <QCheckBox>
#include <QHideEvent>
#include <QScrollArea>
#include <QTimer>
#include <QVBoxLayout>
#include <iostream>

using namespace WhiskerToolbox::MediaProcessing;

// =============================================================================
// Helper: map chain_key → active flag field in MediaDisplayOptions
// =============================================================================

namespace {

/// Read the active flag for a given chain_key from MediaDisplayOptions
bool getActiveFlag(MediaDisplayOptions const & opts, std::string const & chain_key) {
    if (chain_key == "1__lineartransform") return opts.contrast_active;
    if (chain_key == "2__gamma") return opts.gamma_active;
    if (chain_key == "3__sharpen") return opts.sharpen_active;
    if (chain_key == "4__clahe") return opts.clahe_active;
    if (chain_key == "5__bilateral") return opts.bilateral_active;
    if (chain_key == "6__median") return opts.median_active;
    return false;
}

/// Set the active flag for a given chain_key in MediaDisplayOptions
void setActiveFlag(MediaDisplayOptions & opts, std::string const & chain_key, bool active) {
    if (chain_key == "1__lineartransform") opts.contrast_active = active;
    else if (chain_key == "2__gamma")
        opts.gamma_active = active;
    else if (chain_key == "3__sharpen")
        opts.sharpen_active = active;
    else if (chain_key == "4__clahe")
        opts.clahe_active = active;
    else if (chain_key == "5__bilateral")
        opts.bilateral_active = active;
    else if (chain_key == "6__median")
        opts.median_active = active;
}

/// Serialize the options struct for a given chain_key to JSON string
std::string getOptionsJson(MediaDisplayOptions const & opts, std::string const & chain_key) {
    if (chain_key == "1__lineartransform") return rfl::json::write(opts.contrast_options);
    if (chain_key == "2__gamma") return rfl::json::write(opts.gamma_options);
    if (chain_key == "3__sharpen") return rfl::json::write(opts.sharpen_options);
    if (chain_key == "4__clahe") return rfl::json::write(opts.clahe_options);
    if (chain_key == "5__bilateral") return rfl::json::write(opts.bilateral_options);
    if (chain_key == "6__median") return rfl::json::write(opts.median_options);
    return "{}";
}

/// Deserialize JSON into the appropriate options struct field
void setOptionsFromJson(MediaDisplayOptions & opts, std::string const & chain_key, std::string const & json) {
    if (chain_key == "1__lineartransform") {
        auto result = rfl::json::read<ContrastOptions>(json);
        if (result) opts.contrast_options = result.value();
    } else if (chain_key == "2__gamma") {
        auto result = rfl::json::read<GammaOptions>(json);
        if (result) opts.gamma_options = result.value();
    } else if (chain_key == "3__sharpen") {
        auto result = rfl::json::read<SharpenOptions>(json);
        if (result) opts.sharpen_options = result.value();
    } else if (chain_key == "4__clahe") {
        auto result = rfl::json::read<ClaheOptions>(json);
        if (result) opts.clahe_options = result.value();
    } else if (chain_key == "5__bilateral") {
        auto result = rfl::json::read<BilateralOptions>(json);
        if (result) opts.bilateral_options = result.value();
    } else if (chain_key == "6__median") {
        auto result = rfl::json::read<MedianOptions>(json);
        if (result) opts.median_options = result.value();
    }
}

}// anonymous namespace

// =============================================================================
// Construction / Destruction
// =============================================================================

MediaProcessing_Widget::MediaProcessing_Widget(
        std::shared_ptr<DataManager> data_manager,
        Media_Window * scene,
        MediaWidgetState * state,
        QWidget * parent)
    : QWidget(parent),
      ui(new Ui::MediaProcessing_Widget),
      _data_manager{std::move(data_manager)},
      _scene{scene},
      _state{state} {

    ui->setupUi(this);
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);

    _setupRegistrySections();
    _setupMagicEraserSection();
    _setupColormapSection();

    if (_scene) {
        connect(_scene, &Media_Window::leftReleaseDrawing, this, &MediaProcessing_Widget::_onDrawingFinished);
    }

    QTimer::singleShot(0, this, [this]() {
        if (parentWidget()) {
            setMinimumWidth(parentWidget()->width());
            adjustSize();
            if (ui->scrollAreaWidgetContents) {
                ui->scrollAreaWidgetContents->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
            }
        }
    });
}

MediaProcessing_Widget::~MediaProcessing_Widget() {
    delete ui;
}

// =============================================================================
// setActiveKey
// =============================================================================

void MediaProcessing_Widget::setActiveKey(std::string const & key) {
    _active_key = key;
    ui->name_label->setText(QString::fromStdString(key));

    _updateMedianKernelConstraints();
    _loadProcessingChainFromMedia();
}

// =============================================================================
// hideEvent
// =============================================================================

void MediaProcessing_Widget::hideEvent(QHideEvent * event) {
    static_cast<void>(event);
    if (_scene) {
        _scene->setShowHoverCircle(false);
        _scene->setDrawingMode(false);
    }
}

// =============================================================================
// Registry-driven sections (Phase 4 core)
// =============================================================================

void MediaProcessing_Widget::_setupRegistrySections() {
    auto * scroll_layout = ui->scroll_layout;
    auto const & steps = ProcessingStepRegistry::instance().steps();

    for (auto const & step: steps) {
        ProcessingSection ps;
        ps.chain_key = step.chain_key;

        // Section container
        ps.section = new Section(this, QString::fromStdString(step.display_name));
        auto * section_layout = new QVBoxLayout();

        // Active checkbox
        ps.active_checkbox = new QCheckBox(tr("Active"), this);
        section_layout->addWidget(ps.active_checkbox);

        // AutoParamWidget
        ps.param_widget = new AutoParamWidget(this);
        ps.param_widget->setSchema(step.schema);
        section_layout->addWidget(ps.param_widget);

        // Install contrast post-edit hook for bidirectional alpha/beta ↔ min/max
        if (step.chain_key == "1__lineartransform") {
            ps.param_widget->setPostEditHook(&MediaProcessing_Widget::_contrastPostEditHook);
        }

        ps.section->setContentLayout(*section_layout);
        scroll_layout->insertWidget(scroll_layout->count() - 1, ps.section);

        // Capture index for lambdas
        auto const idx = _processing_sections.size();
        _processing_sections.push_back(std::move(ps));
        auto & ref = _processing_sections[idx];

        // Connect signals
        connect(ref.param_widget, &AutoParamWidget::parametersChanged, this, [this, idx]() {
            _onRegistryParamsChanged(_processing_sections[idx]);
        });
        connect(ref.active_checkbox, &QCheckBox::toggled, this, [this, idx](bool active) {
            _onRegistryActiveChanged(_processing_sections[idx], active);
        });
    }
}

void MediaProcessing_Widget::_onRegistryParamsChanged(ProcessingSection const & ps) {
    if (_active_key.empty() || !_state) return;

    auto const key = QString::fromStdString(_active_key);
    auto const * media_opts = _state->displayOptions().get<MediaDisplayOptions>(key);
    if (!media_opts) return;

    auto modified = *media_opts;
    setOptionsFromJson(modified, ps.chain_key, ps.param_widget->toJson());

    // Auto-enable on parameter change
    if (!ps.active_checkbox->isChecked()) {
        ps.active_checkbox->blockSignals(true);
        ps.active_checkbox->setChecked(true);
        ps.active_checkbox->blockSignals(false);
        setActiveFlag(modified, ps.chain_key, true);
    }

    _state->displayOptions().set(key, modified);
    _applyRegistryStep(ps);
}

void MediaProcessing_Widget::_onRegistryActiveChanged(ProcessingSection const & ps, bool active) {
    if (_active_key.empty() || !_state) return;

    auto const key = QString::fromStdString(_active_key);
    auto const * media_opts = _state->displayOptions().get<MediaDisplayOptions>(key);
    if (!media_opts) return;

    auto modified = *media_opts;
    setActiveFlag(modified, ps.chain_key, active);
    _state->displayOptions().set(key, modified);
    _applyRegistryStep(ps);
}

void MediaProcessing_Widget::_applyRegistryStep(ProcessingSection const & ps) {
    if (_active_key.empty()) return;

    auto media_data = _data_manager->getData<MediaData>(_active_key);
    if (!media_data) return;

    auto const * step = ProcessingStepRegistry::instance().findByKey(ps.chain_key);
    if (!step) return;

    if (ps.active_checkbox->isChecked()) {
        auto params_json = nlohmann::json::parse(ps.param_widget->toJson(), nullptr, false);
        if (params_json.is_discarded()) return;

        media_data->addProcessingStep(ps.chain_key, [apply = step->apply, params_json](void * input) {
            auto * mat = static_cast<cv::Mat *>(input);
            apply(*mat, params_json);
        });
    } else {
        media_data->removeProcessingStep(ps.chain_key);
    }

    if (_scene) {
        _scene->UpdateCanvas();
    }
}

// =============================================================================
// Contrast post-edit hook (Phase 5)
// =============================================================================

std::string MediaProcessing_Widget::_contrastPostEditHook(std::string const & json) {
    auto result = rfl::json::read<ContrastOptions>(json);
    if (!result) return json;

    // The hook is called after any field change. We recalculate derived fields.
    // We let the user's edit stand and derive the other pair.
    // Since we can't tell which field changed, we use min/max → alpha/beta
    // because the AutoParamWidget naturally represents all 4 fields.
    auto opts = result.value();
    opts.calculateAlphaBetaFromMinMax();
    return rfl::json::write(opts);
}

// =============================================================================
// Magic Eraser section (Phase 5: special — canvas interaction)
// =============================================================================

void MediaProcessing_Widget::_setupMagicEraserSection() {
    auto * scroll_layout = ui->scroll_layout;

    _magic_eraser_section = new Section(this, tr("Magic Eraser"));
    auto * section_layout = new QVBoxLayout();

    _magic_eraser_active_checkbox = new QCheckBox(tr("Active"), this);
    section_layout->addWidget(_magic_eraser_active_checkbox);

    // AutoParamWidget for MagicEraserParams (brush_size, median_filter_size)
    _magic_eraser_param_widget = new AutoParamWidget(this);
    auto schema = extractParameterSchema<MagicEraserParams>();
    _magic_eraser_param_widget->setSchema(schema);
    section_layout->addWidget(_magic_eraser_param_widget);

    // Custom drawing controls
    _magic_eraser_drawing_widget = new MagicEraserWidget(this);
    section_layout->addWidget(_magic_eraser_drawing_widget);

    _magic_eraser_section->setContentLayout(*section_layout);
    scroll_layout->insertWidget(scroll_layout->count() - 1, _magic_eraser_section);

    connect(_magic_eraser_active_checkbox, &QCheckBox::toggled,
            this, &MediaProcessing_Widget::_onMagicEraserActiveChanged);
    connect(_magic_eraser_param_widget, &AutoParamWidget::parametersChanged,
            this, &MediaProcessing_Widget::_onMagicEraserParamsChanged);
    connect(_magic_eraser_drawing_widget, &MagicEraserWidget::drawingModeChanged,
            this, &MediaProcessing_Widget::_onMagicEraserDrawingModeChanged);
    connect(_magic_eraser_drawing_widget, &MagicEraserWidget::clearMaskRequested,
            this, &MediaProcessing_Widget::_onMagicEraserClearMaskRequested);
}

void MediaProcessing_Widget::_onMagicEraserParamsChanged() {
    if (_active_key.empty() || !_state) return;

    auto const key = QString::fromStdString(_active_key);
    auto const * media_opts = _state->displayOptions().get<MediaDisplayOptions>(key);
    if (!media_opts) return;

    auto modified = *media_opts;
    auto params_json = _magic_eraser_param_widget->toJson();
    auto result = rfl::json::read<MagicEraserParams>(params_json);
    if (result) {
        modified.magic_eraser_params = result.value();
    }

    // Auto-enable
    if (!_magic_eraser_active_checkbox->isChecked()) {
        _magic_eraser_active_checkbox->blockSignals(true);
        _magic_eraser_active_checkbox->setChecked(true);
        _magic_eraser_active_checkbox->blockSignals(false);
        modified.magic_eraser_active = true;
    }

    _state->displayOptions().set(key, modified);
    _applyMagicEraser(modified.magic_eraser_active);
}

void MediaProcessing_Widget::_onMagicEraserActiveChanged(bool active) {
    if (_active_key.empty() || !_state) return;

    auto const key = QString::fromStdString(_active_key);
    auto const * media_opts = _state->displayOptions().get<MediaDisplayOptions>(key);
    if (!media_opts) return;

    auto modified = *media_opts;
    modified.magic_eraser_active = active;
    _state->displayOptions().set(key, modified);
    _applyMagicEraser(active);
}

void MediaProcessing_Widget::_applyMagicEraser(bool active) {
    if (_active_key.empty()) return;

    auto media_data = _data_manager->getData<MediaData>(_active_key);
    if (!media_data) return;

    auto const key = QString::fromStdString(_active_key);
    auto const * media_opts = _state->displayOptions().get<MediaDisplayOptions>(key);
    if (!media_opts) return;

    auto combined = MagicEraserOptions::fromParamsAndState(
            media_opts->magic_eraser_params, media_opts->magic_eraser_state);

    if (active && !combined.mask.empty()) {
        media_data->addProcessingStep("7__magic_eraser", [combined](void * input) {
            auto * mat = static_cast<cv::Mat *>(input);
            ImageProcessing::apply_magic_eraser(*mat, combined);
        });
    } else {
        media_data->removeProcessingStep("7__magic_eraser");
    }

    if (_scene) {
        _scene->UpdateCanvas();
    }
}

void MediaProcessing_Widget::_onMagicEraserDrawingModeChanged(bool enabled) {
    if (_active_key.empty() || !_state) return;

    auto const * media_opts = _state->displayOptions().get<MediaDisplayOptions>(
            QString::fromStdString(_active_key));
    if (!media_opts) return;

    if (_scene && media_opts->magic_eraser_active) {
        _scene->setDrawingMode(enabled);
        if (enabled) {
            _scene->setHoverCircleRadius(media_opts->magic_eraser_params.brush_size);
            _scene->setShowHoverCircle(true);
        } else {
            _scene->setShowHoverCircle(false);
        }
    }
}

void MediaProcessing_Widget::_onMagicEraserClearMaskRequested() {
    if (_active_key.empty() || !_state) return;

    auto const key = QString::fromStdString(_active_key);
    auto const * media_opts = _state->displayOptions().get<MediaDisplayOptions>(key);
    if (!media_opts) return;

    auto modified = *media_opts;
    modified.magic_eraser_state.mask.clear();
    modified.magic_eraser_state.image_size = {0, 0};
    _state->displayOptions().set(key, modified);
    _applyMagicEraser(modified.magic_eraser_active);
}

void MediaProcessing_Widget::_onDrawingFinished() {
    if (_active_key.empty() || !_state) return;

    auto const key = QString::fromStdString(_active_key);
    auto const * media_opts = _state->displayOptions().get<MediaDisplayOptions>(key);
    if (!media_opts) return;

    auto modified = *media_opts;
    auto & eraser_state = modified.magic_eraser_state;

    auto media_data = _data_manager->getData<MediaData>(_active_key);
    if (!media_data) return;

    auto new_mask = _scene->getDrawingMask();
    auto image_size = media_data->getImageSize();

    // Merge masks if compatible
    if (!eraser_state.mask.empty() &&
        eraser_state.image_size.width == image_size.width &&
        eraser_state.image_size.height == image_size.height &&
        eraser_state.mask.size() == new_mask.size()) {
        for (size_t i = 0; i < new_mask.size(); ++i) {
            eraser_state.mask[i] = std::max(eraser_state.mask[i], new_mask[i]);
        }
    } else {
        eraser_state.mask = new_mask;
    }

    eraser_state.image_size = image_size;
    _state->displayOptions().set(key, modified);

    auto combined = MagicEraserOptions::fromParamsAndState(modified.magic_eraser_params, eraser_state);
    _applyMagicEraser(modified.magic_eraser_active);
}

// =============================================================================
// Colormap section (Phase 5: special — rendering, not processing chain)
// =============================================================================

void MediaProcessing_Widget::_setupColormapSection() {
    auto * scroll_layout = ui->scroll_layout;

    _colormap_section = new Section(this, tr("Colormap"));
    auto * section_layout = new QVBoxLayout();

    _colormap_active_checkbox = new QCheckBox(tr("Active"), this);
    section_layout->addWidget(_colormap_active_checkbox);

    _colormap_param_widget = new AutoParamWidget(this);
    auto schema = extractParameterSchema<ColormapOptions>();
    _colormap_param_widget->setSchema(schema);
    section_layout->addWidget(_colormap_param_widget);

    _colormap_section->setContentLayout(*section_layout);
    scroll_layout->insertWidget(scroll_layout->count() - 1, _colormap_section);

    connect(_colormap_active_checkbox, &QCheckBox::toggled,
            this, &MediaProcessing_Widget::_onColormapActiveChanged);
    connect(_colormap_param_widget, &AutoParamWidget::parametersChanged,
            this, &MediaProcessing_Widget::_onColormapParamsChanged);
}

void MediaProcessing_Widget::_onColormapParamsChanged() {
    if (_active_key.empty() || !_state) return;

    auto const key = QString::fromStdString(_active_key);
    auto const * media_opts = _state->displayOptions().get<MediaDisplayOptions>(key);
    if (!media_opts) return;

    auto modified = *media_opts;
    auto result = rfl::json::read<ColormapOptions>(_colormap_param_widget->toJson());
    if (result) {
        modified.colormap_options = result.value();
    }

    // Auto-enable when user changes colormap params
    if (!_colormap_active_checkbox->isChecked()) {
        _colormap_active_checkbox->blockSignals(true);
        _colormap_active_checkbox->setChecked(true);
        _colormap_active_checkbox->blockSignals(false);
        modified.colormap_active = true;
    }

    _state->displayOptions().set(key, modified);
    if (_scene) {
        _scene->UpdateCanvas();
    }
}

void MediaProcessing_Widget::_onColormapActiveChanged(bool active) {
    if (_active_key.empty() || !_state) return;

    auto const key = QString::fromStdString(_active_key);
    auto const * media_opts = _state->displayOptions().get<MediaDisplayOptions>(key);
    if (!media_opts) return;

    auto modified = *media_opts;
    modified.colormap_active = active;
    _state->displayOptions().set(key, modified);
    if (_scene) {
        _scene->UpdateCanvas();
    }
}

void MediaProcessing_Widget::_updateColormapAvailability() {
    if (!_colormap_section || _active_key.empty()) return;

    auto media_data = _data_manager->getData<MediaData>(_active_key);
    if (!media_data) {
        _colormap_section->setEnabled(false);
        return;
    }

    bool const is_grayscale = (media_data->getFormat() == MediaData::DisplayFormat::Gray);
    _colormap_section->setEnabled(is_grayscale);

    if (!is_grayscale && _colormap_active_checkbox->isChecked()) {
        _colormap_active_checkbox->blockSignals(true);
        _colormap_active_checkbox->setChecked(false);
        _colormap_active_checkbox->blockSignals(false);
    }
}

// =============================================================================
// Load processing chain from media (all sections)
// =============================================================================

void MediaProcessing_Widget::_loadProcessingChainFromMedia() {
    if (_active_key.empty() || !_state) return;

    auto media_data = _data_manager->getData<MediaData>(_active_key);
    if (!media_data) return;

    auto const key = QString::fromStdString(_active_key);
    auto const * options = _state->displayOptions().get<MediaDisplayOptions>(key);
    if (!options) return;

    _updateMedianKernelConstraints();

    auto modified = *options;
    bool needs_update = false;

    // Detect active steps from the processing chain
    for (auto const & ps: _processing_sections) {
        if (media_data->hasProcessingStep(ps.chain_key)) {
            setActiveFlag(modified, ps.chain_key, true);
            needs_update = true;
        }
    }
    if (media_data->hasProcessingStep("7__magic_eraser")) {
        modified.magic_eraser_active = true;
        needs_update = true;
    }

    if (needs_update) {
        _state->displayOptions().set(key, modified);
    }

    // Update registry-driven sections
    for (auto const & ps: _processing_sections) {
        ps.param_widget->fromJson(getOptionsJson(modified, ps.chain_key));
        ps.active_checkbox->blockSignals(true);
        ps.active_checkbox->setChecked(getActiveFlag(modified, ps.chain_key));
        ps.active_checkbox->blockSignals(false);
    }

    // Update magic eraser
    if (_magic_eraser_param_widget) {
        auto params_json = rfl::json::write(modified.magic_eraser_params);
        _magic_eraser_param_widget->fromJson(params_json);
        _magic_eraser_active_checkbox->blockSignals(true);
        _magic_eraser_active_checkbox->setChecked(modified.magic_eraser_active);
        _magic_eraser_active_checkbox->blockSignals(false);
        _magic_eraser_drawing_widget->setDrawingMode(modified.magic_eraser_state.drawing_mode);
    }

    // Update colormap
    if (_colormap_param_widget) {
        auto colormap_json = rfl::json::write(modified.colormap_options);
        _colormap_param_widget->fromJson(colormap_json);
        _colormap_active_checkbox->blockSignals(true);
        _colormap_active_checkbox->setChecked(modified.colormap_active);
        _colormap_active_checkbox->blockSignals(false);
    }

    _updateColormapAvailability();
}

// =============================================================================
// Median kernel constraints (Phase 5)
// =============================================================================

void MediaProcessing_Widget::_updateMedianKernelConstraints() {
    if (_active_key.empty()) return;

    auto media_data = _data_manager->getData<MediaData>(_active_key);
    if (!media_data) return;

    bool const is_8bit_grayscale =
            media_data->is8Bit() && (media_data->getFormat() == MediaData::DisplayFormat::Gray);

    // Find the median section and update its schema with adjusted constraints
    for (auto & ps: _processing_sections) {
        if (ps.chain_key != "6__median") continue;

        auto schema = extractParameterSchema<MedianOptions>();
        if (auto * f = schema.field("kernel_size")) {
            f->min_value = 3;
            f->max_value = is_8bit_grayscale ? 21 : 5;
            f->tooltip = "Kernel size for median filter (must be odd, >= 3)";
            if (!is_8bit_grayscale) {
                f->tooltip += " — Max 5 for non-8-bit grayscale (OpenCV)";
            }
        }
        ps.param_widget->setSchema(schema);
        break;
    }
}
