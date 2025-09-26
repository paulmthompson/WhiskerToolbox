#include "MediaProcessing_Widget.hpp"
#include "ui_MediaProcessing_Widget.h"

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
#include "DataManager/Media/Media_Data.hpp"
#include "ImageProcessing/OpenCVUtility.hpp"
#include "Media_Widget/Media_Widget.hpp"
#include "Media_Widget/Media_Window/Media_Window.hpp"

#include <QHideEvent>
#include <QScrollArea>
#include <QTimer>
#include <QVBoxLayout>
#include <iostream>

MediaProcessing_Widget::MediaProcessing_Widget(std::shared_ptr<DataManager> data_manager, Media_Window * scene, QWidget * parent)
    : QWidget(parent),
      ui(new Ui::MediaProcessing_Widget),
      _data_manager{std::move(data_manager)},
      _scene{scene},
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

    // Add colormap section to the scroll layout (before the spacer)
    scroll_layout->insertWidget(scroll_layout->count() - 1, _colormap_section);

    // Set initial colormap options
    //_colormap_widget->setOptions(_colormap_options);
}

void MediaProcessing_Widget::_onContrastOptionsChanged(ContrastOptions const & options) {
    if (!_active_key.empty()) {
        auto media_opts = _scene->getMediaConfig(_active_key);
        if (media_opts.has_value()) {
            media_opts.value()->contrast_options = options;
            _applyContrastFilter(options);
        }
    }

    std::cout << "Contrast options changed - Active: " << options.active
              << ", Alpha: " << options.alpha << ", Beta: " << options.beta << std::endl;
}

void MediaProcessing_Widget::_onGammaOptionsChanged(GammaOptions const & options) {
    if (!_active_key.empty()) {
        auto media_opts = _scene->getMediaConfig(_active_key);
        if (media_opts.has_value()) {
            media_opts.value()->gamma_options = options;
            _applyGammaFilter(options);
        }
    }

    std::cout << "Gamma options changed - Active: " << options.active
              << ", Gamma: " << options.gamma << std::endl;
}

void MediaProcessing_Widget::_onSharpenOptionsChanged(SharpenOptions const & options) {
    if (!_active_key.empty()) {
        auto media_opts = _scene->getMediaConfig(_active_key);
        if (media_opts.has_value()) {
            media_opts.value()->sharpen_options = options;
            _applySharpenFilter(options);
        }
    }

    std::cout << "Sharpen options changed - Active: " << options.active
              << ", Sigma: " << options.sigma << std::endl;
}

void MediaProcessing_Widget::_onClaheOptionsChanged(ClaheOptions const & options) {
    if (!_active_key.empty()) {
        auto media_opts = _scene->getMediaConfig(_active_key);
        if (media_opts.has_value()) {
            media_opts.value()->clahe_options = options;
            _applyClaheFilter(options);
        }
    }

    std::cout << "CLAHE options changed - Active: " << options.active
              << ", Clip Limit: " << options.clip_limit << ", Grid Size: " << options.grid_size << std::endl;
}

void MediaProcessing_Widget::_onBilateralOptionsChanged(BilateralOptions const & options) {
    if (!_active_key.empty()) {
        auto media_opts = _scene->getMediaConfig(_active_key);
        if (media_opts.has_value()) {
            media_opts.value()->bilateral_options = options;
            _applyBilateralFilter(options);
        }
    }

    std::cout << "Bilateral options changed - Active: " << options.active
              << ", D: " << options.diameter << ", Color Sigma: " << options.sigma_color
              << ", Space Sigma: " << options.sigma_spatial << std::endl;
}

void MediaProcessing_Widget::_onMedianOptionsChanged(MedianOptions const & options) {
    if (!_active_key.empty()) {
        auto media_opts = _scene->getMediaConfig(_active_key);
        if (media_opts.has_value()) {
            media_opts.value()->median_options = options;
            _applyMedianFilter(options);
        }
    }

    std::cout << "Median options changed - Active: " << options.active
              << ", Kernel Size: " << options.kernel_size << std::endl;
}

void MediaProcessing_Widget::_onMagicEraserOptionsChanged(MagicEraserOptions const & options) {
    if (!_active_key.empty()) {
        auto media_opts = _scene->getMediaConfig(_active_key);
        if (media_opts.has_value()) {
            media_opts.value()->magic_eraser_options = options;
            if (!options.active) {
                media_opts.value()->magic_eraser_options.mask.clear();
                media_opts.value()->magic_eraser_options.image_size = {0, 0};
            }
            _applyMagicEraser(options);
        }
    }

    std::cout << "Magic Eraser options changed - Active: " << options.active
              << ", Brush Size: " << options.brush_size << ", Median Filter Size: " << options.median_filter_size
              << ", Drawing Mode: " << options.drawing_mode << std::endl;
}

void MediaProcessing_Widget::_onMagicEraserDrawingModeChanged(bool enabled) {

    if (!_active_key.empty()) {
        auto media_opts = _scene->getMediaConfig(_active_key);
        if (media_opts.has_value()) {
            auto magic_eraser_options = media_opts.value()->magic_eraser_options;
            if (_scene && magic_eraser_options.active) {
                _scene->setDrawingMode(enabled);
                if (enabled) {
                    _scene->setHoverCircleRadius(magic_eraser_options.brush_size);
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

    if (!_active_key.empty()) {
        auto media_opts = _scene->getMediaConfig(_active_key);
        if (media_opts.has_value()) {
            auto magic_eraser_options = media_opts.value()->magic_eraser_options;
            magic_eraser_options.mask.clear();
            magic_eraser_options.image_size = {0, 0};

            _applyMagicEraser(magic_eraser_options);
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

    if (_active_key.empty()) {
        return;
    }
    auto media_opts = _scene->getMediaConfig(_active_key);
    if (!media_opts.has_value()) {
        return;
    }

    auto magic_eraser_options = media_opts.value()->magic_eraser_options;

    auto media_data = _data_manager->getData<MediaData>(_active_key);
    if (!media_data) {
        return;
    }

    // Get the drawing mask and image size
    auto new_mask = _scene->getDrawingMask();
    auto image_size = media_data->getImageSize();

    // If we already have a mask stored, merge the new drawing with the existing mask
    if (!magic_eraser_options.mask.empty() &&
        magic_eraser_options.image_size.width == image_size.width &&
        magic_eraser_options.image_size.height == image_size.height) {

        // Merge new mask with existing mask using bitwise OR operation
        // Both masks should be the same size
        if (magic_eraser_options.mask.size() == new_mask.size()) {
            for (size_t i = 0; i < new_mask.size(); ++i) {
                // Combine masks: if either pixel is non-zero, result is non-zero
                magic_eraser_options.mask[i] = std::max(magic_eraser_options.mask[i], new_mask[i]);
            }
            std::cout << "Magic eraser: Merged new drawing with existing mask" << std::endl;
        } else {
            // Size mismatch, replace with new mask
            magic_eraser_options.mask = new_mask;
            std::cout << "Magic eraser: Size mismatch, replaced existing mask" << std::endl;
        }
    } else {
        // No existing mask or different image size, use the new mask
        magic_eraser_options.mask = new_mask;
        std::cout << "Magic eraser: Created new mask" << std::endl;
    }

    // Update the stored image size
    magic_eraser_options.image_size = image_size;

    // Apply the magic eraser to the process chain
    _applyMagicEraser(magic_eraser_options);

    std::cout << "Magic eraser mask stored and applied to process chain. Mask size: "
              << magic_eraser_options.mask.size() << " pixels" << std::endl;
    std::cout << "The number of non zero pixels in the mask is: "
              << std::count_if(magic_eraser_options.mask.begin(), magic_eraser_options.mask.end(), [](uint8_t pixel) { return pixel != 0; }) << std::endl;
}

void MediaProcessing_Widget::_applyContrastFilter(ContrastOptions const & options) {
    if (_active_key.empty()) return;

    auto media_data = _data_manager->getData<MediaData>(_active_key);
    if (!media_data) return;

    if (options.active) {
        // Add or update the contrast filter in the processing chain using the options structure
        media_data->addProcessingStep("1__lineartransform", [options](void * input) {
            cv::Mat * mat = static_cast<cv::Mat *>(input);
            ImageProcessing::linear_transform(*mat, options);
        });
    } else {
        // Remove the contrast filter from the processing chain
        media_data->removeProcessingStep("1__lineartransform");
    }

    // Update the canvas
    if (_scene) {
        _scene->UpdateCanvas();
    }
}

void MediaProcessing_Widget::_applyGammaFilter(GammaOptions const & options) {
    if (_active_key.empty()) return;

    auto media_data = _data_manager->getData<MediaData>(_active_key);
    if (!media_data) return;

    if (options.active) {
        // Add or update the gamma filter in the processing chain using the options structure
        media_data->addProcessingStep("2__gamma", [options](void * input) {
            cv::Mat * mat = static_cast<cv::Mat *>(input);
            ImageProcessing::gamma_transform(*mat, options);
        });
    } else {
        // Remove the gamma filter from the processing chain
        media_data->removeProcessingStep("2__gamma");
    }

    // Update the canvas
    if (_scene) {
        _scene->UpdateCanvas();
    }
}

void MediaProcessing_Widget::_applySharpenFilter(SharpenOptions const & options) {
    if (_active_key.empty()) return;

    auto media_data = _data_manager->getData<MediaData>(_active_key);
    if (!media_data) return;

    if (options.active) {
        // Add or update the sharpen filter in the processing chain using the options structure
        media_data->addProcessingStep("3__sharpen", [options](void * input) {
            cv::Mat * mat = static_cast<cv::Mat *>(input);
            ImageProcessing::sharpen_image(*mat, options);
        });
    } else {
        // Remove the sharpen filter from the processing chain
        media_data->removeProcessingStep("3__sharpen");
    }

    // Update the canvas
    if (_scene) {
        _scene->UpdateCanvas();
    }
}

void MediaProcessing_Widget::_applyClaheFilter(ClaheOptions const & options) {
    if (_active_key.empty()) return;

    auto media_data = _data_manager->getData<MediaData>(_active_key);
    if (!media_data) return;

    if (options.active) {
        // Add or update the CLAHE filter in the processing chain using the options structure
        media_data->addProcessingStep("4__clahe", [options](void * input) {
            cv::Mat * mat = static_cast<cv::Mat *>(input);
            ImageProcessing::clahe(*mat, options);
        });
    } else {
        // Remove the CLAHE filter from the processing chain
        media_data->removeProcessingStep("4__clahe");
    }

    // Update the canvas
    if (_scene) {
        _scene->UpdateCanvas();
    }
}

void MediaProcessing_Widget::_applyBilateralFilter(BilateralOptions const & options) {
    if (_active_key.empty()) return;

    auto media_data = _data_manager->getData<MediaData>(_active_key);
    if (!media_data) return;

    if (options.active) {
        // Add or update the bilateral filter in the processing chain using the options structure
        media_data->addProcessingStep("5__bilateral", [options](void * input) {
            cv::Mat * mat = static_cast<cv::Mat *>(input);
            ImageProcessing::bilateral_filter(*mat, options);
        });
    } else {
        // Remove the bilateral filter from the processing chain
        media_data->removeProcessingStep("5__bilateral");
    }

    // Update the canvas
    if (_scene) {
        _scene->UpdateCanvas();
    }
}

void MediaProcessing_Widget::_applyMedianFilter(MedianOptions const & options) {
    if (_active_key.empty()) return;

    auto media_data = _data_manager->getData<MediaData>(_active_key);
    if (!media_data) return;

    if (options.active) {
        // Add or update the median filter in the processing chain using the options structure
        media_data->addProcessingStep("6__median", [options](void * input) {
            cv::Mat * mat = static_cast<cv::Mat *>(input);
            ImageProcessing::median_filter(*mat, options);
        });
    } else {
        // Remove the median filter from the processing chain
        media_data->removeProcessingStep("6__median");
    }

    // Update the canvas
    if (_scene) {
        _scene->UpdateCanvas();
    }
}

void MediaProcessing_Widget::_applyMagicEraser(MagicEraserOptions const & options) {
    if (_active_key.empty()) return;

    auto media_data = _data_manager->getData<MediaData>(_active_key);
    if (!media_data) return;

    if (options.active && !options.mask.empty()) {
        // Add or update the magic eraser filter in the processing chain
        media_data->addProcessingStep("7__magic_eraser", [options](void * input) {
            cv::Mat * mat = static_cast<cv::Mat *>(input);
            ImageProcessing::apply_magic_eraser(*mat, options);
        });
    } else {
        // Remove the magic eraser filter from the processing chain
        media_data->removeProcessingStep("7__magic_eraser");
    }

    // Update the canvas
    if (_scene) {
        _scene->UpdateCanvas();
    }

    std::cout << "Magic eraser process chain updated - Active: " << options.active
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
    bool is_grayscale = (media_data->getFormat() == MediaData::DisplayFormat::Gray);
    _colormap_widget->setColormapEnabled(is_grayscale);

    std::cout << "Colormap availability updated - Grayscale: " << (is_grayscale ? "YES" : "NO") << std::endl;
}

void MediaProcessing_Widget::_onColormapOptionsChanged(ColormapOptions const & options) {
    
    
    if (!_active_key.empty()) {
        auto media_opts = _scene->getMediaConfig(_active_key);
        if (media_opts.has_value()) {
            media_opts.value()->colormap_options = options;
            _scene->UpdateCanvas();
        }

        std::cout << "Colormap options changed for media '" << _active_key
                  << "' - Active: " << options.active
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

    auto options_opt = _scene->getMediaConfig(_active_key);
    if (!options_opt) {
        std::cout << "No media config found for key: " << _active_key << std::endl;
        return;
    }
    auto options = options_opt.value();

    // Before applying options, ensure UI constraints reflect current media properties
    _updateMedianKernelConstraints();

    // Check each processing step and update options accordingly
    // Note: The actual parameter values are stored in the lambda closures
    // so we can only determine if steps are active, not their exact parameters
    // To properly reconstruct parameters, the MediaData would need to store them separately

    if (media_data->hasProcessingStep("1__contrast")) {
        options->contrast_options.active = true;
        std::cout << "Found contrast processing step" << std::endl;
    }

    if (media_data->hasProcessingStep("2__gamma")) {
        options->gamma_options.active = true;
        std::cout << "Found gamma processing step" << std::endl;
    }

    if (media_data->hasProcessingStep("3__sharpen")) {
        options->sharpen_options.active = true;
        std::cout << "Found sharpen processing step" << std::endl;
    }

    if (media_data->hasProcessingStep("4__clahe")) {
        options->clahe_options.active = true;
        std::cout << "Found CLAHE processing step" << std::endl;
    }

    if (media_data->hasProcessingStep("5__bilateral")) {
        options->bilateral_options.active = true;
        std::cout << "Found bilateral processing step" << std::endl;
    }

    if (media_data->hasProcessingStep("6__median")) {
        options->median_options.active = true;
        std::cout << "Found median processing step" << std::endl;
    }

    if (media_data->hasProcessingStep("7__magic_eraser")) {
        options->magic_eraser_options.active = true;
        std::cout << "Found magic eraser processing step" << std::endl;
    }

    // Update all UI widgets with the loaded options
    if (_contrast_widget) {
        _contrast_widget->setOptions(options->contrast_options);
    }
    if (_gamma_widget) {
        _gamma_widget->setOptions(options->gamma_options);
    }
    if (_sharpen_widget) {
        _sharpen_widget->setOptions(options->sharpen_options);
    }
    if (_clahe_widget) {
        _clahe_widget->setOptions(options->clahe_options);
    }
    if (_bilateral_widget) {
        _bilateral_widget->setOptions(options->bilateral_options);
    }
    if (_median_widget) {
        _median_widget->setOptions(options->median_options);
    }
    if (_magic_eraser_widget) {
        _magic_eraser_widget->setOptions(options->magic_eraser_options);
    }
    if (_colormap_widget) {
        _colormap_widget->setOptions(options->colormap_options);
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

    bool is_8bit_grayscale = media_data->is8Bit() && (media_data->getFormat() == MediaData::DisplayFormat::Gray);
    _median_widget->setKernelConstraints(is_8bit_grayscale);

    std::cout << "Median kernel constraints updated - 8-bit grayscale: "
              << (is_8bit_grayscale ? "YES" : "NO") << std::endl;
}
