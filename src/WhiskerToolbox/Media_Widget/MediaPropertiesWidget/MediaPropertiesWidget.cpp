#include "MediaPropertiesWidget.hpp"
#include "ui_MediaPropertiesWidget.h"

#include "Collapsible_Widget/Section.hpp"
#include "DataManager/DataManager.hpp"
#include "Media_Widget/MediaWidgetState.hpp"
#include "Media_Widget/MediaInterval_Widget/MediaInterval_Widget.hpp"
#include "Media_Widget/MediaLine_Widget/MediaLine_Widget.hpp"
#include "Media_Widget/MediaMask_Widget/MediaMask_Widget.hpp"
#include "Media_Widget/MediaPoint_Widget/MediaPoint_Widget.hpp"
#include "Media_Widget/MediaProcessing_Widget/MediaProcessing_Widget.hpp"
#include "Media_Widget/MediaTensor_Widget/MediaTensor_Widget.hpp"
#include "Media_Widget/MediaText_Widget/MediaText_Widget.hpp"
#include "Media_Window/Media_Window.hpp"

#include <QTimer>
#include <QVBoxLayout>

#include <iostream>

MediaPropertiesWidget::MediaPropertiesWidget(std::shared_ptr<MediaWidgetState> state,
                                               std::shared_ptr<DataManager> data_manager,
                                               Media_Window * media_window,
                                               QWidget * parent)
    : QWidget(parent),
      ui(new Ui::MediaPropertiesWidget),
      _state(std::move(state)),
      _data_manager(std::move(data_manager)),
      _media_window(media_window) {
    ui->setupUi(this);

    _setupTextOverlays();
    _setupFeatureTable();
    _createStackedWidgets();
    _connectTextWidgetToScene();
    _connectStateSignals();
}

MediaPropertiesWidget::~MediaPropertiesWidget() {
    // Proactively hide stacked pages while _media_window is still valid
    if (ui && ui->stackedWidget) {
        for (int i = 0; i < ui->stackedWidget->count(); ++i) {
            QWidget * w = ui->stackedWidget->widget(i);
            if (w && w->isVisible()) {
                w->hide();
            }
        }
    }

    delete ui;
}

void MediaPropertiesWidget::setMediaWindow(Media_Window * media_window) {
    _media_window = media_window;
    
    // Connect text widget to the new media window
    _connectTextWidgetToScene();
    
    // Update existing stacked widgets with the new Media_Window reference
    // The widgets need the media window for drawing operations
    if (_media_window && ui->stackedWidget) {
        // Recreate stacked widgets with new media window
        // Clear existing widgets
        while (ui->stackedWidget->count() > 1) {
            QWidget * w = ui->stackedWidget->widget(ui->stackedWidget->count() - 1);
            ui->stackedWidget->removeWidget(w);
            delete w;
        }
        
        _createStackedWidgets();
    }
}

void MediaPropertiesWidget::_setupTextOverlays() {
    // Create text overlay section (collapsible)
    _text_section = new Section(this, "Text Overlays");
    _text_widget = new MediaText_Widget(this);
    _text_section->setContentLayout(*new QVBoxLayout());
    _text_section->layout()->addWidget(_text_widget);
    _text_section->autoSetContentLayout();

    // Insert text section at the beginning of the content layout
    // (before the feature table widget)
    ui->contentLayout->insertWidget(0, _text_section);
}

void MediaPropertiesWidget::_connectTextWidgetToScene() {
    if (_media_window && _text_widget) {
        _media_window->setTextWidget(_text_widget);

        // Connect text widget signals to update canvas when overlays change
        connect(_text_widget, &MediaText_Widget::textOverlayAdded, 
                _media_window, &Media_Window::UpdateCanvas);
        connect(_text_widget, &MediaText_Widget::textOverlayRemoved, 
                _media_window, &Media_Window::UpdateCanvas);
        connect(_text_widget, &MediaText_Widget::textOverlayUpdated, 
                _media_window, &Media_Window::UpdateCanvas);
        connect(_text_widget, &MediaText_Widget::textOverlaysCleared, 
                _media_window, &Media_Window::UpdateCanvas);
    }
}

void MediaPropertiesWidget::_setupFeatureTable() {
    if (!_data_manager) {
        return;
    }

    ui->feature_table_widget->setColumns({"Feature", "Enabled", "Type"});
    ui->feature_table_widget->setTypeFilter({
        DM_DataType::Line, 
        DM_DataType::Mask, 
        DM_DataType::Points, 
        DM_DataType::DigitalInterval, 
        DM_DataType::Tensor, 
        DM_DataType::Video, 
        DM_DataType::Images
    });
    ui->feature_table_widget->setDataManager(_data_manager);
    ui->feature_table_widget->populateTable();

    // Connect feature table signals
    connect(ui->feature_table_widget, &Feature_Table_Widget::featureSelected,
            this, &MediaPropertiesWidget::_featureSelected);

    connect(ui->feature_table_widget, &Feature_Table_Widget::addFeature,
            this, [this](QString const & feature) {
                _addFeatureToDisplay(feature, true);
            });

    connect(ui->feature_table_widget, &Feature_Table_Widget::removeFeature,
            this, [this](QString const & feature) {
                _addFeatureToDisplay(feature, false);
            });

    // Sizing adjustments after layout
    QTimer::singleShot(0, this, [this]() {
        int scrollAreaWidth = ui->scrollArea->width();
        ui->feature_table_widget->setFixedWidth(scrollAreaWidth - 10);
        ui->stackedWidget->setFixedWidth(scrollAreaWidth - 10);
    });
}

void MediaPropertiesWidget::_createStackedWidgets() {
    if (!_data_manager || !_state) {
        return;
    }

    // Only create data-type editing widgets if we have a valid media window
    // These widgets require Media_Window for drawing operations
    if (!_media_window) {
        return;
    }

    // Create data-type specific editing widgets
    // Index 0 is the empty placeholder page from the UI file
    ui->stackedWidget->addWidget(new MediaPoint_Widget(_data_manager, _media_window, _state.get()));
    ui->stackedWidget->addWidget(new MediaLine_Widget(_data_manager, _media_window, _state.get()));
    ui->stackedWidget->addWidget(new MediaMask_Widget(_data_manager, _media_window, _state.get()));
    ui->stackedWidget->addWidget(new MediaInterval_Widget(_data_manager, _media_window, _state.get()));
    ui->stackedWidget->addWidget(new MediaTensor_Widget(_data_manager, _media_window, _state.get()));

    // Create and store reference to MediaProcessing_Widget
    _processing_widget = new MediaProcessing_Widget(_data_manager, _media_window, _state.get());
    ui->stackedWidget->addWidget(_processing_widget);

    // Size widgets properly after a short delay
    QTimer::singleShot(100, this, [this]() {
        int scrollAreaWidth = ui->scrollArea->width();
        for (int i = 0; i < ui->stackedWidget->count(); ++i) {
            QWidget * widget = ui->stackedWidget->widget(i);
            if (widget) {
                widget->setFixedWidth(scrollAreaWidth - 10);
                widget->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);

                // If this is a MediaProcessing_Widget, make sure it fills its container
                auto processingWidget = qobject_cast<MediaProcessing_Widget *>(widget);
                if (processingWidget) {
                    processingWidget->setMinimumWidth(scrollAreaWidth - 10);
                    processingWidget->adjustSize();
                }
            }
        }
    });
}

void MediaPropertiesWidget::_featureSelected(QString const & feature) {
    if (!_data_manager) {
        ui->stackedWidget->setCurrentIndex(0);
        return;
    }

    auto const type = _data_manager->getType(feature.toStdString());
    auto key = feature.toStdString();

    // Stacked widget indices:
    // 0 = empty placeholder page
    // 1 = MediaPoint_Widget
    // 2 = MediaLine_Widget
    // 3 = MediaMask_Widget
    // 4 = MediaInterval_Widget
    // 5 = MediaTensor_Widget
    // 6 = MediaProcessing_Widget

    if (type == DM_DataType::Points) {
        int const stacked_widget_index = 1;
        ui->stackedWidget->setCurrentIndex(stacked_widget_index);
        auto point_widget = dynamic_cast<MediaPoint_Widget *>(ui->stackedWidget->widget(stacked_widget_index));
        if (point_widget) {
            point_widget->setActiveKey(key);
        }

    } else if (type == DM_DataType::Line) {
        int const stacked_widget_index = 2;
        ui->stackedWidget->setCurrentIndex(stacked_widget_index);
        auto line_widget = dynamic_cast<MediaLine_Widget *>(ui->stackedWidget->widget(stacked_widget_index));
        if (line_widget) {
            line_widget->setActiveKey(key);
        }

    } else if (type == DM_DataType::Mask) {
        int const stacked_widget_index = 3;
        ui->stackedWidget->setCurrentIndex(stacked_widget_index);
        auto mask_widget = dynamic_cast<MediaMask_Widget *>(ui->stackedWidget->widget(stacked_widget_index));
        if (mask_widget) {
            mask_widget->setActiveKey(key);
        }

    } else if (type == DM_DataType::DigitalInterval) {
        int const stacked_widget_index = 4;
        ui->stackedWidget->setCurrentIndex(stacked_widget_index);
        auto interval_widget = dynamic_cast<MediaInterval_Widget *>(ui->stackedWidget->widget(stacked_widget_index));
        if (interval_widget) {
            interval_widget->setActiveKey(key);
        }

    } else if (type == DM_DataType::Tensor) {
        int const stacked_widget_index = 5;
        ui->stackedWidget->setCurrentIndex(stacked_widget_index);
        auto tensor_widget = dynamic_cast<MediaTensor_Widget *>(ui->stackedWidget->widget(stacked_widget_index));
        if (tensor_widget) {
            tensor_widget->setActiveKey(key);
        }

    } else if (type == DM_DataType::Video || type == DM_DataType::Images) {
        int const stacked_widget_index = 6;
        ui->stackedWidget->setCurrentIndex(stacked_widget_index);
        auto processing_widget = dynamic_cast<MediaProcessing_Widget *>(ui->stackedWidget->widget(stacked_widget_index));
        if (processing_widget) {
            processing_widget->setActiveKey(key);
        }

    } else {
        ui->stackedWidget->setCurrentIndex(0);
        std::cout << "Unsupported feature type" << std::endl;
    }

    // Update state and emit signal
    if (_state) {
        _state->setDisplayedDataKey(feature);
    }
    emit featureSelected(feature);
}

void MediaPropertiesWidget::_addFeatureToDisplay(QString const & feature, bool enabled) {
    emit featureEnabledChanged(feature, enabled);
}

void MediaPropertiesWidget::_connectStateSignals() {
    if (!_state) {
        return;
    }

    // Listen for external state changes (e.g., from workspace restore)
    // and update UI accordingly
    
    // Connect to displayedDataKeyChanged to sync feature table selection
    connect(_state.get(), &MediaWidgetState::displayedDataKeyChanged,
            this, [this](QString const & key) {
                // Could highlight the feature in the table if needed
                // For now, the table manages its own selection
            });
}
