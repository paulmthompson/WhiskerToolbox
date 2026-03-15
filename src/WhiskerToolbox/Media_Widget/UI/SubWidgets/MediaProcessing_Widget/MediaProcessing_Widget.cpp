#include "MediaProcessing_Widget.hpp"
#include "ui_MediaProcessing_Widget.h"

#include "Media_Widget/Core/MediaWidgetState.hpp"
#include "Media_Widget/Rendering/Media_Window/Media_Window.hpp"
#include "Media_Widget/UI/Media_Widget.hpp"
#include "ProcessingOptions/BilateralWidget.hpp"
#include "ProcessingOptions/ClaheWidget.hpp"
#include "ProcessingOptions/ColormapWidget.hpp"
#include "ProcessingOptions/ContrastWidget.hpp"
#include "ProcessingOptions/GammaWidget.hpp"
#include "ProcessingOptions/MagicEraserWidget.hpp"
#include "ProcessingOptions/MedianWidget.hpp"
#include "ProcessingOptions/SharpenWidget.hpp"

#include "Collapsible_Widget/Section.hpp"
#include "DataManager/DataManager.hpp"
#include "ImageProcessing/OpenCVUtility.hpp"
#include "Media/Media_Data.hpp"


#include <QHideEvent>
#include <QScrollArea>
#include <QTimer>
#include <QVBoxLayout>
#include <iostream>

MediaProcessing_Widget::MediaProcessing_Widget(std::shared_ptr<DataManager> data_manager, Media_Window * scene, MediaWidgetState * state, QWidget * parent)
    : QWidget(parent),
      ui(new Ui::MediaProcessing_Widget),
      _data_manager{std::move(data_manager)},
      _scene{scene},
      _state{state},
      _contrast_widget(nullptr),
      _contrast_section(nullptr),
      _gamma_widget(nullptr),
      _gamma_section(nullptr),
      _sharpen_widget(nullptr),
      _sharpen_section(nullptr),
      _clahe_widget(nullptr),
      _clahe_section(nullptr),
      _bilateral_widget(nullptr),
      _bilateral_section(nullptr),
      _median_widget(nullptr),
      _median_section(nullptr),
      _magic_eraser_widget(nullptr),
      _magic_eraser_section(nullptr),
      _colormap_widget(nullptr),
      _colormap_section(nullptr) {

    ui->setupUi(this);

    // Set size policy to expand to fill available space
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);

    _setupProcessingWidgets();

    if (_scene) {
        connect(_scene, &Media_Window::leftReleaseDrawing, this, &MediaProcessing_Widget::_onDrawingFinished);
    }

    // Use a timer to adjust size after the UI has been fully initialized
    QTimer::singleShot(0, this, [this]() {
        if (parentWidget()) {
            // Resize to match parent widget width
            setMinimumWidth(parentWidget()->width());
            adjustSize();

            // Make the scroll area widget contents expand properly
            if (ui->scrollAreaWidgetContents) {
                ui->scrollAreaWidgetContents->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
            }
        }
    });
}

MediaProcessing_Widget::~MediaProcessing_Widget() {
    delete ui;
}

void MediaProcessing_Widget::setActiveKey(std::string const & key) {
    _active_key = key;
    ui->name_label->setText(QString::fromStdString(key));

    std::cout << "MediaProcessing_Widget active key set to: " << key << std::endl;

    // Update constraints that depend on media properties
    _updateMedianKernelConstraints();

    // Load the processing chain from the selected media
    _loadProcessingChainFromMedia();
}

void MediaProcessing_Widget::_setupProcessingWidgets() {
    // Get the scroll area's content layout
    auto * scroll_layout = ui->scroll_layout;

    // Create contrast section
    _contrast_widget = new ContrastWidget(this);
    _contrast_section = new Section(this, "Linear Transform");
    _contrast_section->setContentLayout(*new QVBoxLayout());
    _contrast_section->layout()->addWidget(_contrast_widget);
    _contrast_section->autoSetContentLayout();

    // Connect contrast widget signals
    connect(_contrast_widget, &ContrastWidget::optionsChanged,
            this, &MediaProcessing_Widget::_onContrastOptionsChanged);
    connect(_contrast_widget, &ContrastWidget::activeChanged,
            this, &MediaProcessing_Widget::_onContrastActiveChanged);

    // Add contrast section to the scroll layout (before the spacer)
    scroll_layout->insertWidget(scroll_layout->count() - 1, _contrast_section);

    // Set initial contrast options
    //_contrast_widget->setOptions(_contrast_options);

    // Create gamma section
    _gamma_widget = new GammaWidget(this);
    _gamma_section = new Section(this, "Gamma Correction");
    _gamma_section->setContentLayout(*new QVBoxLayout());
    _gamma_section->layout()->addWidget(_gamma_widget);
    _gamma_section->autoSetContentLayout();

    // Connect gamma widget signals
    connect(_gamma_widget, &GammaWidget::optionsChanged,
            this, &MediaProcessing_Widget::_onGammaOptionsChanged);
    connect(_gamma_widget, &GammaWidget::activeChanged,
            this, &MediaProcessing_Widget::_onGammaActiveChanged);

    // Add gamma section to the scroll layout (before the spacer)
    scroll_layout->insertWidget(scroll_layout->count() - 1, _gamma_section);

    // Set initial gamma options
    //_gamma_widget->setOptions(_gamma_options);

    // Create sharpen section
    _sharpen_widget = new SharpenWidget(this);
    _sharpen_section = new Section(this, "Image Sharpening");
    _sharpen_section->setContentLayout(*new QVBoxLayout());
    _sharpen_section->layout()->addWidget(_sharpen_widget);
    _sharpen_section->autoSetContentLayout();

    // Connect sharpen widget signals
    connect(_sharpen_widget, &SharpenWidget::optionsChanged,
            this, &MediaProcessing_Widget::_onSharpenOptionsChanged);
    connect(_sharpen_widget, &SharpenWidget::activeChanged,
            this, &MediaProcessing_Widget::_onSharpenActiveChanged);

    // Add sharpen section to the scroll layout (before the spacer)
    scroll_layout->insertWidget(scroll_layout->count() - 1, _sharpen_section);

    // Set initial sharpen options
    //_sharpen_widget->setOptions(_sharpen_options);

    // Create clahe section
    _clahe_widget = new ClaheWidget(this);
    _clahe_section = new Section(this, "CLAHE");
    _clahe_section->setContentLayout(*new QVBoxLayout());
    _clahe_section->layout()->addWidget(_clahe_widget);
    _clahe_section->autoSetContentLayout();

    // Connect clahe widget signals
    connect(_clahe_widget, &ClaheWidget::optionsChanged,
            this, &MediaProcessing_Widget::_onClaheOptionsChanged);
    connect(_clahe_widget, &ClaheWidget::activeChanged,
            this, &MediaProcessing_Widget::_onClaheActiveChanged);

    // Add clahe section to the scroll layout (before the spacer)
    scroll_layout->insertWidget(scroll_layout->count() - 1, _clahe_section);

    // Set initial clahe options
    //_clahe_widget->setOptions(_clahe_options);

    // Create bilateral section
    _bilateral_widget = new BilateralWidget(this);
    _bilateral_section = new Section(this, "Bilateral Filter");
    _bilateral_section->setContentLayout(*new QVBoxLayout());
    _bilateral_section->layout()->addWidget(_bilateral_widget);
    _bilateral_section->autoSetContentLayout();

    // Connect bilateral widget signals
    connect(_bilateral_widget, &BilateralWidget::optionsChanged,
            this, &MediaProcessing_Widget::_onBilateralOptionsChanged);
    connect(_bilateral_widget, &BilateralWidget::activeChanged,
            this, &MediaProcessing_Widget::_onBilateralActiveChanged);

    // Add bilateral section to the scroll layout (before the spacer)
    scroll_layout->insertWidget(scroll_layout->count() - 1, _bilateral_section);

    // Set initial bilateral options
    //_bilateral_widget->setOptions(_bilateral_options);

    // Create median section
    _median_widget = new MedianWidget(this);
    _median_section = new Section(this, "Median Filter");
    _median_section->setContentLayout(*new QVBoxLayout());
    _median_section->layout()->addWidget(_median_widget);
    _median_section->autoSetContentLayout();

    // Connect median widget signals
    connect(_median_widget, &MedianWidget::optionsChanged,
            this, &MediaProcessing_Widget::_onMedianOptionsChanged);
    connect(_median_widget, &MedianWidget::activeChanged,
            this, &MediaProcessing_Widget::_onMedianActiveChanged);

    // Add median section to the scroll layout (before the spacer)
    scroll_layout->insertWidget(scroll_layout->count() - 1, _median_section);

    // Set initial median options
    //_median_widget->setOptions(_median_options);

    // Create magic eraser section
    _magic_eraser_widget = new MagicEraserWidget(this);
    _magic_eraser_section = new Section(this, "Magic Eraser");
    _magic_eraser_section->setContentLayout(*new QVBoxLayout());
    _magic_eraser_section->layout()->addWidget(_magic_eraser_widget);
    _magic_eraser_section->autoSetContentLayout();

    // Connect magic eraser widget signals
    connect(_magic_eraser_widget, &MagicEraserWidget::optionsChanged,
            this, &MediaProcessing_Widget::_onMagicEraserOptionsChanged);
    connect(_magic_eraser_widget, &MagicEraserWidget::activeChanged,
            this, &MediaProcessing_Widget::_onMagicEraserActiveChanged);
    connect(_magic_eraser_widget, &MagicEraserWidget::drawingModeChanged,
            this, &MediaProcessing_Widget::_onMagicEraserDrawingModeChanged);
    connect(_magic_eraser_widget, &MagicEraserWidget::clearMaskRequested,
            this, &MediaProcessing_Widget::_onMagicEraserClearMaskRequested);

    // Add magic eraser section to the scroll layout (before the spacer)
    scroll_layout->insertWidget(scroll_layout->count() - 1, _magic_eraser_section);

    // Set initial magic eraser options
    //_magic_eraser_widget->setOptions(_magic_eraser_options);

    // Create colormap section
    _colormap_widget = new ColormapWidget(this);
    _colormap_section = new Section(this, "Colormap");
    _colormap_section->setContentLayout(*new QVBoxLayout());
    _colormap_section->layout()->addWidget(_colormap_widget);
    _colormap_section->autoSetContentLayout();

    // Connect colormap widget signals
    connect(_colormap_widget, &ColormapWidget::optionsChanged,
            this, &MediaProcessing_Widget::_onColormapOptionsChanged);
    connect(_colormap_widget, &ColormapWidget::activeChanged,
            this, &MediaProcessing_Widget::_onColormapActiveChanged);

    // Add colormap section to the scroll layout (before the spacer)
    scroll_layout->insertWidget(scroll_layout->count() - 1, _colormap_section);

    // Set initial colormap options
    //_colormap_widget->setOptions(_colormap_options);
}

void MediaProcessing_Widget::_onContrastOptionsChanged(ContrastOptions const & options) {
    if (!_active_key.empty() && _state) {
        auto const key = QString::fromStdString(_active_key);
        if (auto const * media_opts = _state->displayOptions().get<MediaDisplayOptions>(key); media_opts) {
            auto modified = *media_opts;
            modified.contrast_options = options;
            _state->displayOptions().set(key, modified);
            _applyContrastFilter(options, modified.contrast_active);
        }
    }

    std::cout << "Contrast options changed"
              << ", Alpha: " << options.alpha << ", Beta: " << options.beta << std::endl;
}

void MediaProcessing_Widget::_onGammaOptionsChanged(GammaOptions const & options) {
    if (!_active_key.empty() && _state) {
        auto const key = QString::fromStdString(_active_key);
        if (auto const * media_opts = _state->displayOptions().get<MediaDisplayOptions>(key); media_opts) {
            auto modified = *media_opts;
            modified.gamma_options = options;
            _state->displayOptions().set(key, modified);
            _applyGammaFilter(options, modified.gamma_active);
        }
    }

    std::cout << "Gamma options changed"
              << ", Gamma: " << options.gamma << std::endl;
}

void MediaProcessing_Widget::_onSharpenOptionsChanged(SharpenOptions const & options) {
    if (!_active_key.empty() && _state) {
        auto const key = QString::fromStdString(_active_key);
        if (auto const * media_opts = _state->displayOptions().get<MediaDisplayOptions>(key); media_opts) {
            auto modified = *media_opts;
            modified.sharpen_options = options;
            _state->displayOptions().set(key, modified);
            _applySharpenFilter(options, modified.sharpen_active);
        }
    }

    std::cout << "Sharpen options changed"
              << ", Sigma: " << options.sigma << std::endl;
}

void MediaProcessing_Widget::_onClaheOptionsChanged(ClaheOptions const & options) {
    if (!_active_key.empty() && _state) {
        auto const key = QString::fromStdString(_active_key);
        if (auto const * media_opts = _state->displayOptions().get<MediaDisplayOptions>(key); media_opts) {
            auto modified = *media_opts;
            modified.clahe_options = options;
            _state->displayOptions().set(key, modified);
            _applyClaheFilter(options, modified.clahe_active);
        }
    }

    std::cout << "CLAHE options changed"
              << ", Clip Limit: " << options.clip_limit << ", Grid Size: " << options.grid_size << std::endl;
}

void MediaProcessing_Widget::_onBilateralOptionsChanged(BilateralOptions const & options) {
    if (!_active_key.empty() && _state) {
        auto const key = QString::fromStdString(_active_key);
        if (auto const * media_opts = _state->displayOptions().get<MediaDisplayOptions>(key); media_opts) {
            auto modified = *media_opts;
            modified.bilateral_options = options;
            _state->displayOptions().set(key, modified);
            _applyBilateralFilter(options, modified.bilateral_active);
        }
    }

    std::cout << "Bilateral options changed"
              << ", D: " << options.diameter << ", Color Sigma: " << options.sigma_color
              << ", Space Sigma: " << options.sigma_spatial << std::endl;
}

void MediaProcessing_Widget::_onMedianOptionsChanged(MedianOptions const & options) {
    if (!_active_key.empty() && _state) {
        auto const key = QString::fromStdString(_active_key);
        if (auto const * media_opts = _state->displayOptions().get<MediaDisplayOptions>(key); media_opts) {
            auto modified = *media_opts;
            modified.median_options = options;
            _state->displayOptions().set(key, modified);
            _applyMedianFilter(options, modified.median_active);
        }
    }

    std::cout << "Median options changed"
              << ", Kernel Size: " << options.kernel_size << std::endl;
}

void MediaProcessing_Widget::_onMagicEraserOptionsChanged(MagicEraserOptions const & options) {
    if (!_active_key.empty() && _state) {
        auto const key = QString::fromStdString(_active_key);
        if (auto const * media_opts = _state->displayOptions().get<MediaDisplayOptions>(key); media_opts) {
            auto modified = *media_opts;
            modified.magic_eraser_params.brush_size = options.brush_size;
            modified.magic_eraser_params.median_filter_size = options.median_filter_size;
            modified.magic_eraser_state.drawing_mode = options.drawing_mode;
            if (!modified.magic_eraser_active) {
                modified.magic_eraser_state.mask.clear();
                modified.magic_eraser_state.image_size = {0, 0};
            }
            _state->displayOptions().set(key, modified);
            auto combined = MagicEraserOptions::fromParamsAndState(modified.magic_eraser_params, modified.magic_eraser_state);
            _applyMagicEraser(combined, modified.magic_eraser_active);
        }
    }

    std::cout << "Magic Eraser options changed"
              << ", Brush Size: " << options.brush_size << ", Median Filter Size: " << options.median_filter_size
              << ", Drawing Mode: " << options.drawing_mode << std::endl;
}

void MediaProcessing_Widget::_onMagicEraserDrawingModeChanged(bool enabled) {

    if (!_active_key.empty() && _state) {
        auto const * media_opts = _state->displayOptions().get<MediaDisplayOptions>(QString::fromStdString(_active_key));
        if (media_opts) {
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
    }

    std::cout << "Magic Eraser drawing mode changed to: " << (enabled ? "enabled" : "disabled") << std::endl;
}

void MediaProcessing_Widget::_onMagicEraserClearMaskRequested() {

    if (!_active_key.empty() && _state) {
        auto const key = QString::fromStdString(_active_key);
        if (auto const * media_opts = _state->displayOptions().get<MediaDisplayOptions>(key); media_opts) {
            auto modified = *media_opts;
            modified.magic_eraser_state.mask.clear();
            modified.magic_eraser_state.image_size = {0, 0};
            _state->displayOptions().set(key, modified);

            auto combined = MagicEraserOptions::fromParamsAndState(modified.magic_eraser_params, modified.magic_eraser_state);
            _applyMagicEraser(combined, modified.magic_eraser_active);
        }
    }

    std::cout << "Magic eraser mask cleared" << std::endl;
}

void MediaProcessing_Widget::hideEvent(QHideEvent * event) {
    static_cast<void>(event);

    std::cout << "MediaProcessing_Widget Hide Event" << std::endl;

    // Clean up hover circle when switching away from processing widget
    if (_scene) {
        _scene->setShowHoverCircle(false);
        _scene->setDrawingMode(false);
    }
}

void MediaProcessing_Widget::_onDrawingFinished() {
    // Only apply magic eraser if it's active and in drawing mode

    if (_active_key.empty() || !_state) {
        return;
    }
    auto const key = QString::fromStdString(_active_key);
    auto const * media_opts = _state->displayOptions().get<MediaDisplayOptions>(key);
    if (!media_opts) {
        return;
    }

    auto modified = *media_opts;
    auto & eraser_state = modified.magic_eraser_state;

    auto media_data = _data_manager->getData<MediaData>(_active_key);
    if (!media_data) {
        return;
    }

    // Get the drawing mask and image size
    auto new_mask = _scene->getDrawingMask();
    auto image_size = media_data->getImageSize();

    // If we already have a mask stored, merge the new drawing with the existing mask
    if (!eraser_state.mask.empty() &&
        eraser_state.image_size.width == image_size.width &&
        eraser_state.image_size.height == image_size.height) {

        // Merge new mask with existing mask using bitwise OR operation
        // Both masks should be the same size
        if (eraser_state.mask.size() == new_mask.size()) {
            for (size_t i = 0; i < new_mask.size(); ++i) {
                // Combine masks: if either pixel is non-zero, result is non-zero
                eraser_state.mask[i] = std::max(eraser_state.mask[i], new_mask[i]);
            }
            std::cout << "Magic eraser: Merged new drawing with existing mask" << std::endl;
        } else {
            // Size mismatch, replace with new mask
            eraser_state.mask = new_mask;
            std::cout << "Magic eraser: Size mismatch, replaced existing mask" << std::endl;
        }
    } else {
        // No existing mask or different image size, use the new mask
        eraser_state.mask = new_mask;
        std::cout << "Magic eraser: Created new mask" << std::endl;
    }

    // Update the stored image size
    eraser_state.image_size = image_size;

    // Save modified options back to state
    _state->displayOptions().set(key, modified);

    // Apply the magic eraser to the process chain
    auto combined = MagicEraserOptions::fromParamsAndState(modified.magic_eraser_params, eraser_state);
    _applyMagicEraser(combined, modified.magic_eraser_active);

    std::cout << "Magic eraser mask stored and applied to process chain. Mask size: "
              << eraser_state.mask.size() << " pixels" << std::endl;
    std::cout << "The number of non zero pixels in the mask is: "
              << std::count_if(eraser_state.mask.begin(), eraser_state.mask.end(), [](uint8_t pixel) { return pixel != 0; }) << std::endl;
}

void MediaProcessing_Widget::_applyContrastFilter(ContrastOptions const & options, bool active) {
    if (_active_key.empty()) return;

    auto media_data = _data_manager->getData<MediaData>(_active_key);
    if (!media_data) return;

    if (active) {
        media_data->addProcessingStep("1__lineartransform", [options](void * input) {
            auto * mat = static_cast<cv::Mat *>(input);
            ImageProcessing::linear_transform(*mat, options);
        });
    } else {
        media_data->removeProcessingStep("1__lineartransform");
    }

    if (_scene) {
        _scene->UpdateCanvas();
    }
}

void MediaProcessing_Widget::_applyGammaFilter(GammaOptions const & options, bool active) {
    if (_active_key.empty()) return;

    auto media_data = _data_manager->getData<MediaData>(_active_key);
    if (!media_data) return;

    if (active) {
        media_data->addProcessingStep("2__gamma", [options](void * input) {
            auto * mat = static_cast<cv::Mat *>(input);
            ImageProcessing::gamma_transform(*mat, options);
        });
    } else {
        media_data->removeProcessingStep("2__gamma");
    }

    if (_scene) {
        _scene->UpdateCanvas();
    }
}

void MediaProcessing_Widget::_applySharpenFilter(SharpenOptions const & options, bool active) {
    if (_active_key.empty()) return;

    auto media_data = _data_manager->getData<MediaData>(_active_key);
    if (!media_data) return;

    if (active) {
        media_data->addProcessingStep("3__sharpen", [options](void * input) {
            auto * mat = static_cast<cv::Mat *>(input);
            ImageProcessing::sharpen_image(*mat, options);
        });
    } else {
        media_data->removeProcessingStep("3__sharpen");
    }

    if (_scene) {
        _scene->UpdateCanvas();
    }
}

void MediaProcessing_Widget::_applyClaheFilter(ClaheOptions const & options, bool active) {
    if (_active_key.empty()) return;

    auto media_data = _data_manager->getData<MediaData>(_active_key);
    if (!media_data) return;

    if (active) {
        media_data->addProcessingStep("4__clahe", [options](void * input) {
            auto * mat = static_cast<cv::Mat *>(input);
            ImageProcessing::clahe(*mat, options);
        });
    } else {
        media_data->removeProcessingStep("4__clahe");
    }

    if (_scene) {
        _scene->UpdateCanvas();
    }
}

void MediaProcessing_Widget::_applyBilateralFilter(BilateralOptions const & options, bool active) {
    if (_active_key.empty()) return;

    auto media_data = _data_manager->getData<MediaData>(_active_key);
    if (!media_data) return;

    if (active) {
        media_data->addProcessingStep("5__bilateral", [options](void * input) {
            auto * mat = static_cast<cv::Mat *>(input);
            ImageProcessing::bilateral_filter(*mat, options);
        });
    } else {
        media_data->removeProcessingStep("5__bilateral");
    }

    if (_scene) {
        _scene->UpdateCanvas();
    }
}

void MediaProcessing_Widget::_applyMedianFilter(MedianOptions const & options, bool active) {
    if (_active_key.empty()) return;

    auto media_data = _data_manager->getData<MediaData>(_active_key);
    if (!media_data) return;

    if (active) {
        media_data->addProcessingStep("6__median", [options](void * input) {
            auto * mat = static_cast<cv::Mat *>(input);
            ImageProcessing::median_filter(*mat, options);
        });
    } else {
        media_data->removeProcessingStep("6__median");
    }

    if (_scene) {
        _scene->UpdateCanvas();
    }
}

void MediaProcessing_Widget::_applyMagicEraser(MagicEraserOptions const & options, bool active) {
    if (_active_key.empty()) return;

    auto media_data = _data_manager->getData<MediaData>(_active_key);
    if (!media_data) return;

    if (active && !options.mask.empty()) {
        media_data->addProcessingStep("7__magic_eraser", [options](void * input) {
            auto * mat = static_cast<cv::Mat *>(input);
            ImageProcessing::apply_magic_eraser(*mat, options);
        });
    } else {
        media_data->removeProcessingStep("7__magic_eraser");
    }

    if (_scene) {
        _scene->UpdateCanvas();
    }

    std::cout << "Magic eraser process chain updated - Active: " << active
              << ", Has mask: " << (!options.mask.empty()) << std::endl;
}

void MediaProcessing_Widget::_updateColormapAvailability() {
    if (!_colormap_widget || _active_key.empty()) {
        return;
    }

    auto media_data = _data_manager->getData<MediaData>(_active_key);
    if (!media_data) {
        _colormap_widget->setColormapEnabled(false);
        return;
    }

    // Enable colormap only for grayscale images
    bool const is_grayscale = (media_data->getFormat() == MediaData::DisplayFormat::Gray);
    _colormap_widget->setColormapEnabled(is_grayscale);

    std::cout << "Colormap availability updated - Grayscale: " << (is_grayscale ? "YES" : "NO") << std::endl;
}

void MediaProcessing_Widget::_onColormapOptionsChanged(ColormapOptions const & options) {
    if (!_active_key.empty() && _state) {
        auto const key = QString::fromStdString(_active_key);
        if (auto const * media_opts = _state->displayOptions().get<MediaDisplayOptions>(key); media_opts) {
            auto modified = *media_opts;
            modified.colormap_options = options;
            _state->displayOptions().set(key, modified);
            _scene->UpdateCanvas();
        }

        std::cout << "Colormap options changed for media '" << _active_key
                  << "' - Active: " << _colormap_widget->isActive()
                  << ", Type: " << static_cast<int>(options.colormap)
                  << ", Alpha: " << options.alpha << std::endl;
    }
}

void MediaProcessing_Widget::_loadProcessingChainFromMedia() {
    if (_active_key.empty()) {
        std::cout << "No active key set, cannot load processing chain" << std::endl;
        return;
    }

    auto media_data = _data_manager->getData<MediaData>(_active_key);
    if (!media_data) {
        std::cout << "No media data found for key: " << _active_key << std::endl;
        return;
    }

    std::cout << "Loading processing chain from media key: " << _active_key << std::endl;
    std::cout << "Media has " << media_data->getProcessingStepCount() << " processing steps" << std::endl;

    if (!_state) {
        std::cout << "No state available for key: " << _active_key << std::endl;
        return;
    }
    auto const key = QString::fromStdString(_active_key);
    auto const * options = _state->displayOptions().get<MediaDisplayOptions>(key);
    if (!options) {
        std::cout << "No media config found for key: " << _active_key << std::endl;
        return;
    }

    // Before applying options, ensure UI constraints reflect current media properties
    _updateMedianKernelConstraints();

    // Check each processing step and update options accordingly
    // Note: The actual parameter values are stored in the lambda closures
    // so we can only determine if steps are active, not their exact parameters
    // To properly reconstruct parameters, the MediaData would need to store them separately
    // We need to modify the options if any processing steps are found
    auto modified = *options;
    bool needs_update = false;

    if (media_data->hasProcessingStep("1__contrast")) {
        modified.contrast_active = true;
        needs_update = true;
        std::cout << "Found contrast processing step" << std::endl;
    }

    if (media_data->hasProcessingStep("2__gamma")) {
        modified.gamma_active = true;
        needs_update = true;
        std::cout << "Found gamma processing step" << std::endl;
    }

    if (media_data->hasProcessingStep("3__sharpen")) {
        modified.sharpen_active = true;
        needs_update = true;
        std::cout << "Found sharpen processing step" << std::endl;
    }

    if (media_data->hasProcessingStep("4__clahe")) {
        modified.clahe_active = true;
        needs_update = true;
        std::cout << "Found CLAHE processing step" << std::endl;
    }

    if (media_data->hasProcessingStep("5__bilateral")) {
        modified.bilateral_active = true;
        needs_update = true;
        std::cout << "Found bilateral processing step" << std::endl;
    }

    if (media_data->hasProcessingStep("6__median")) {
        modified.median_active = true;
        needs_update = true;
        std::cout << "Found median processing step" << std::endl;
    }

    if (media_data->hasProcessingStep("7__magic_eraser")) {
        modified.magic_eraser_active = true;
        needs_update = true;
        std::cout << "Found magic eraser processing step" << std::endl;
    }

    // Save back if any processing steps were found
    if (needs_update) {
        _state->displayOptions().set(key, modified);
    }

    // Update all UI widgets with the loaded options
    if (_contrast_widget) {
        _contrast_widget->setOptions(modified.contrast_options);
        _contrast_widget->setActive(modified.contrast_active);
    }
    if (_gamma_widget) {
        _gamma_widget->setOptions(modified.gamma_options);
        _gamma_widget->setActive(modified.gamma_active);
    }
    if (_sharpen_widget) {
        _sharpen_widget->setOptions(modified.sharpen_options);
        _sharpen_widget->setActive(modified.sharpen_active);
    }
    if (_clahe_widget) {
        _clahe_widget->setOptions(modified.clahe_options);
        _clahe_widget->setActive(modified.clahe_active);
    }
    if (_bilateral_widget) {
        _bilateral_widget->setOptions(modified.bilateral_options);
        _bilateral_widget->setActive(modified.bilateral_active);
    }
    if (_median_widget) {
        _median_widget->setOptions(modified.median_options);
        _median_widget->setActive(modified.median_active);
    }
    if (_magic_eraser_widget) {
        auto eraser_opts = MagicEraserOptions::fromParamsAndState(
                modified.magic_eraser_params, modified.magic_eraser_state);
        _magic_eraser_widget->setOptions(eraser_opts);
        _magic_eraser_widget->setActive(modified.magic_eraser_active);
    }
    if (_colormap_widget) {
        _colormap_widget->setOptions(modified.colormap_options);
        _colormap_widget->setActive(modified.colormap_active);
    }

    // Update colormap availability based on media format
    _updateColormapAvailability();

    std::cout << "Processing chain loaded and UI updated" << std::endl;
}

// New helper to update median filter UI constraints based on media bit depth and format
void MediaProcessing_Widget::_updateMedianKernelConstraints() {
    if (!_median_widget || _active_key.empty()) {
        return;
    }

    auto media_data = _data_manager->getData<MediaData>(_active_key);
    if (!media_data) {
        return;
    }

    bool const is_8bit_grayscale = media_data->is8Bit() && (media_data->getFormat() == MediaData::DisplayFormat::Gray);
    _median_widget->setKernelConstraints(is_8bit_grayscale);

    std::cout << "Median kernel constraints updated - 8-bit grayscale: "
              << (is_8bit_grayscale ? "YES" : "NO") << std::endl;
}

void MediaProcessing_Widget::_onContrastActiveChanged(bool active) {
    if (!_active_key.empty() && _state) {
        auto const key = QString::fromStdString(_active_key);
        if (auto const * media_opts = _state->displayOptions().get<MediaDisplayOptions>(key); media_opts) {
            auto modified = *media_opts;
            modified.contrast_active = active;
            _state->displayOptions().set(key, modified);
            _applyContrastFilter(modified.contrast_options, active);
        }
    }
}

void MediaProcessing_Widget::_onGammaActiveChanged(bool active) {
    if (!_active_key.empty() && _state) {
        auto const key = QString::fromStdString(_active_key);
        if (auto const * media_opts = _state->displayOptions().get<MediaDisplayOptions>(key); media_opts) {
            auto modified = *media_opts;
            modified.gamma_active = active;
            _state->displayOptions().set(key, modified);
            _applyGammaFilter(modified.gamma_options, active);
        }
    }
}

void MediaProcessing_Widget::_onSharpenActiveChanged(bool active) {
    if (!_active_key.empty() && _state) {
        auto const key = QString::fromStdString(_active_key);
        if (auto const * media_opts = _state->displayOptions().get<MediaDisplayOptions>(key); media_opts) {
            auto modified = *media_opts;
            modified.sharpen_active = active;
            _state->displayOptions().set(key, modified);
            _applySharpenFilter(modified.sharpen_options, active);
        }
    }
}

void MediaProcessing_Widget::_onClaheActiveChanged(bool active) {
    if (!_active_key.empty() && _state) {
        auto const key = QString::fromStdString(_active_key);
        if (auto const * media_opts = _state->displayOptions().get<MediaDisplayOptions>(key); media_opts) {
            auto modified = *media_opts;
            modified.clahe_active = active;
            _state->displayOptions().set(key, modified);
            _applyClaheFilter(modified.clahe_options, active);
        }
    }
}

void MediaProcessing_Widget::_onBilateralActiveChanged(bool active) {
    if (!_active_key.empty() && _state) {
        auto const key = QString::fromStdString(_active_key);
        if (auto const * media_opts = _state->displayOptions().get<MediaDisplayOptions>(key); media_opts) {
            auto modified = *media_opts;
            modified.bilateral_active = active;
            _state->displayOptions().set(key, modified);
            _applyBilateralFilter(modified.bilateral_options, active);
        }
    }
}

void MediaProcessing_Widget::_onMedianActiveChanged(bool active) {
    if (!_active_key.empty() && _state) {
        auto const key = QString::fromStdString(_active_key);
        if (auto const * media_opts = _state->displayOptions().get<MediaDisplayOptions>(key); media_opts) {
            auto modified = *media_opts;
            modified.median_active = active;
            _state->displayOptions().set(key, modified);
            _applyMedianFilter(modified.median_options, active);
        }
    }
}

void MediaProcessing_Widget::_onMagicEraserActiveChanged(bool active) {
    if (!_active_key.empty() && _state) {
        auto const key = QString::fromStdString(_active_key);
        if (auto const * media_opts = _state->displayOptions().get<MediaDisplayOptions>(key); media_opts) {
            auto modified = *media_opts;
            modified.magic_eraser_active = active;
            _state->displayOptions().set(key, modified);

            auto eraser_opts = MagicEraserOptions::fromParamsAndState(
                    modified.magic_eraser_params, modified.magic_eraser_state);
            _applyMagicEraser(eraser_opts, active);
        }
    }
}

void MediaProcessing_Widget::_onColormapActiveChanged(bool active) {
    if (!_active_key.empty() && _state) {
        auto const key = QString::fromStdString(_active_key);
        if (auto const * media_opts = _state->displayOptions().get<MediaDisplayOptions>(key); media_opts) {
            auto modified = *media_opts;
            modified.colormap_active = active;
            _state->displayOptions().set(key, modified);
            _scene->UpdateCanvas();
        }
    }
}
