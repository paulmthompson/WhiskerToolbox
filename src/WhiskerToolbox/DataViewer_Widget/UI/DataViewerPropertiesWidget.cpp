#include "DataViewerPropertiesWidget.hpp"
#include "ui_DataViewerPropertiesWidget.h"

#include "Core/DataViewerState.hpp"
#include "Core/DataViewerStateData.hpp"
#include "Feature_Tree_Model.hpp"
#include "Rendering/OpenGLWidget.hpp"
#include "SubWidgets/AnalogTimeSeries/AnalogViewer_Widget.hpp"
#include "SubWidgets/DigitalEvent/EventViewer_Widget.hpp"
#include "SubWidgets/DigitalInterval/IntervalViewer_Widget.hpp"

#include "DataManager/DataManager.hpp"
#include "Feature_Tree_Widget/Feature_Tree_Widget.hpp"
#include "TimeFrame/TimeFrame.hpp"

#include <QCheckBox>
#include <QComboBox>
#include <QDoubleSpinBox>
#include <QMenu>
#include <QPushButton>
#include <QSpinBox>
#include <QTreeWidget>

#include <iostream>

DataViewerPropertiesWidget::DataViewerPropertiesWidget(std::shared_ptr<DataViewerState> state,
                                                         std::shared_ptr<DataManager> data_manager,
                                                         OpenGLWidget * opengl_widget,
                                                         QWidget * parent)
    : QWidget(parent),
      ui(new Ui::DataViewerPropertiesWidget),
      _state(std::move(state)),
      _data_manager(std::move(data_manager)),
      _opengl_widget(opengl_widget) {
    ui->setupUi(this);

    // Initialize feature tree model
    _feature_tree_model = std::make_unique<Feature_Tree_Model>(this);
    _feature_tree_model->setDataManager(_data_manager);

    _setupFeatureTree();
    
    if (_opengl_widget) {
        _setupStackedWidget();
    }

    _initializeFromState();
    _connectUIControls();
    _connectStateSignals();
}

DataViewerPropertiesWidget::~DataViewerPropertiesWidget() {
    delete ui;
}

void DataViewerPropertiesWidget::setOpenGLWidget(OpenGLWidget * opengl_widget) {
    if (_opengl_widget == opengl_widget) {
        return;
    }
    _opengl_widget = opengl_widget;
    
    if (_opengl_widget) {
        _setupStackedWidget();
    }
}

void DataViewerPropertiesWidget::setTimeFrame(std::shared_ptr<TimeFrame> time_frame) {
    _time_frame = std::move(time_frame);
}

void DataViewerPropertiesWidget::refreshFeatureTree() {
    ui->feature_tree_widget->refreshTree();
}

void DataViewerPropertiesWidget::setXAxisSamplesMaximum(int max) {
    ui->x_axis_samples->setMaximum(max);
}

void DataViewerPropertiesWidget::_initializeFromState() {
    if (!_state) {
        return;
    }
    
    _updating_from_state = true;
    
    // Theme
    int theme_index = 0;
    switch (_state->theme()) {
        case DataViewerTheme::Dark:
            theme_index = 0;
            break;
        case DataViewerTheme::Light:
            theme_index = 2;
            break;
        default:
            theme_index = 0;
            break;
    }
    ui->theme_combo->setCurrentIndex(theme_index);
    
    // Global zoom
    ui->global_zoom->setValue(static_cast<double>(_state->globalZoom()));
    
    // X axis samples (time width from view state)
    auto const & view = _state->viewState();
    ui->x_axis_samples->setValue(static_cast<int>(view.getTimeWidth()));
    
    // Grid settings
    ui->grid_lines_enabled->setChecked(_state->gridEnabled());
    ui->grid_spacing->setValue(_state->gridSpacing());
    
    _updating_from_state = false;
}

void DataViewerPropertiesWidget::_connectUIControls() {
    // Theme combo box
    connect(ui->theme_combo, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &DataViewerPropertiesWidget::_onThemeChanged);
    
    // Global zoom
    connect(ui->global_zoom, &QDoubleSpinBox::valueChanged,
            this, &DataViewerPropertiesWidget::_onGlobalZoomChanged);
    
    // X axis samples
    connect(ui->x_axis_samples, QOverload<int>::of(&QSpinBox::valueChanged),
            this, &DataViewerPropertiesWidget::_onXAxisSamplesChanged);
    
    // Grid controls
    connect(ui->grid_lines_enabled, &QCheckBox::toggled,
            this, &DataViewerPropertiesWidget::_onGridLinesToggled);
    connect(ui->grid_spacing, QOverload<int>::of(&QSpinBox::valueChanged),
            this, &DataViewerPropertiesWidget::_onGridSpacingChanged);
    
    // Auto-arrange button
    connect(ui->auto_arrange_button, &QPushButton::clicked,
            this, &DataViewerPropertiesWidget::autoArrangeRequested);
    
    // Export SVG button
    connect(ui->export_svg_button, &QPushButton::clicked, this, [this]() {
        bool const includeScalebar = ui->svg_scalebar_checkbox->isChecked();
        int const scalebarLength = ui->scalebar_length_spinbox->value();
        emit exportSVGRequested(includeScalebar, scalebarLength);
    });
    
    // Scalebar checkbox enables/disables the length spinbox
    connect(ui->svg_scalebar_checkbox, &QCheckBox::toggled, this, [this](bool checked) {
        ui->scalebar_length_spinbox->setEnabled(checked);
        ui->scalebar_length_label->setEnabled(checked);
    });
}

void DataViewerPropertiesWidget::_connectStateSignals() {
    if (!_state) {
        return;
    }

    // Update theme combo when state changes
    connect(_state.get(), &DataViewerState::themeChanged, this, [this]() {
        if (_updating_from_state) return;
        _updating_from_state = true;
        
        int theme_index = 0;
        switch (_state->theme()) {
            case DataViewerTheme::Dark:
                theme_index = 0;
                break;
            case DataViewerTheme::Light:
                theme_index = 2;
                break;
            default:
                theme_index = 0;
                break;
        }
        ui->theme_combo->setCurrentIndex(theme_index);
        
        _updating_from_state = false;
    });

    // Update grid controls when state changes
    connect(_state.get(), &DataViewerState::gridChanged, this, [this]() {
        if (_updating_from_state) return;
        _updating_from_state = true;
        
        ui->grid_lines_enabled->setChecked(_state->gridEnabled());
        ui->grid_spacing->setValue(_state->gridSpacing());
        
        _updating_from_state = false;
    });

    // Update view controls when state changes
    connect(_state.get(), &DataViewerState::viewStateChanged, this, [this]() {
        if (_updating_from_state) return;
        _updating_from_state = true;
        
        ui->global_zoom->setValue(static_cast<double>(_state->globalZoom()));
        auto const & view = _state->viewState();
        ui->x_axis_samples->setValue(static_cast<int>(view.getTimeWidth()));
        
        _updating_from_state = false;
    });
}

void DataViewerPropertiesWidget::_onThemeChanged(int index) {
    if (_updating_from_state || !_state) return;
    
    DataViewerTheme theme = DataViewerTheme::Dark;
    switch (index) {
        case 0:
            theme = DataViewerTheme::Dark;
            break;
        case 1:
            // Purple theme - map to Dark for now (keeping UI consistent)
            theme = DataViewerTheme::Dark;
            break;
        case 2:
            theme = DataViewerTheme::Light;
            break;
        default:
            theme = DataViewerTheme::Dark;
            break;
    }
    
    _state->setTheme(theme);
}

void DataViewerPropertiesWidget::_onGlobalZoomChanged(double value) {
    if (_updating_from_state || !_state) return;
    
    _state->setGlobalZoom(static_cast<float>(value));
}

void DataViewerPropertiesWidget::_onXAxisSamplesChanged(int value) {
    if (_updating_from_state || !_state) return;
    
    _state->setTimeWidth(value);
}

void DataViewerPropertiesWidget::_onGridLinesToggled(bool enabled) {
    if (_updating_from_state || !_state) return;
    
    _state->setGridEnabled(enabled);
}

void DataViewerPropertiesWidget::_onGridSpacingChanged(int value) {
    if (_updating_from_state || !_state) return;
    
    _state->setGridSpacing(value);
}

void DataViewerPropertiesWidget::_handleFeatureSelected(QString const & feature) {
    if (!_data_manager || feature.isEmpty()) {
        return;
    }

    std::string const key = feature.toStdString();
    auto const type = _data_manager->getType(key);

    // Stacked widget indices: 0=Analog, 1=Interval, 2=Event
    int constexpr stacked_widget_analog_index = 0;
    int constexpr stacked_widget_interval_index = 1;
    int constexpr stacked_widget_event_index = 2;

    if (type == DM_DataType::Analog) {
        ui->stackedWidget->setCurrentIndex(stacked_widget_analog_index);
        auto * analog_widget = dynamic_cast<AnalogViewer_Widget *>(ui->stackedWidget->widget(stacked_widget_analog_index));
        if (analog_widget) {
            analog_widget->setActiveKey(key);
        }
    } else if (type == DM_DataType::DigitalInterval) {
        ui->stackedWidget->setCurrentIndex(stacked_widget_interval_index);
        auto * interval_widget = dynamic_cast<IntervalViewer_Widget *>(ui->stackedWidget->widget(stacked_widget_interval_index));
        if (interval_widget) {
            interval_widget->setActiveKey(key);
        }
    } else if (type == DM_DataType::DigitalEvent) {
        ui->stackedWidget->setCurrentIndex(stacked_widget_event_index);
        auto * event_widget = dynamic_cast<EventViewer_Widget *>(ui->stackedWidget->widget(stacked_widget_event_index));
        if (event_widget) {
            event_widget->setActiveKey(key);
        }
    }
}

void DataViewerPropertiesWidget::_handleColorChanged(std::string const & feature_key, std::string const & hex_color) {
    emit featureColorChanged(feature_key, hex_color);
}

void DataViewerPropertiesWidget::_setupFeatureTree() {
    // Configure Feature_Tree_Widget
    ui->feature_tree_widget->setTypeFilters({DM_DataType::Analog, DM_DataType::DigitalEvent, DM_DataType::DigitalInterval});
    ui->feature_tree_widget->setDataManager(_data_manager);

    // Connect Feature_Tree_Widget signals
    connect(ui->feature_tree_widget, &Feature_Tree_Widget::featureSelected, this, [this](std::string const & feature) {
        _handleFeatureSelected(QString::fromStdString(feature));
    });

    connect(ui->feature_tree_widget, &Feature_Tree_Widget::addFeature, this, [this](std::string const & feature) {
        std::cout << "Properties: Adding single feature: " << feature << std::endl;
        std::string color = _feature_tree_model->getFeatureColor(feature);
        emit featureAddRequested(feature, color);
    });

    // Install context menu handling on the embedded tree widget
    ui->feature_tree_widget->treeWidget()->setContextMenuPolicy(Qt::CustomContextMenu);
    connect(ui->feature_tree_widget->treeWidget(), &QTreeWidget::customContextMenuRequested, this, [this](QPoint const & pos) {
        auto * tw = ui->feature_tree_widget->treeWidget();
        QTreeWidgetItem * item = tw->itemAt(pos);
        if (!item) return;
        std::string const key = item->text(0).toStdString();
        // Group items have children and are under Analog data type parent
        bool const hasChildren = item->childCount() > 0;
        if (hasChildren) {
            // Determine if parent is Analog data type or children are analog keys
            bool isAnalogGroup = false;
            if (auto * parent = item->parent()) {
                if (parent->text(0) == QString::fromStdString(convert_data_type_to_string(DM_DataType::Analog))) {
                    isAnalogGroup = true;
                }
            }
            if (isAnalogGroup) {
                QPoint const global_pos = tw->viewport()->mapToGlobal(pos);
                emit groupContextMenuRequested(key, global_pos);
            }
        }
    });

    connect(ui->feature_tree_widget, &Feature_Tree_Widget::removeFeature, this, [this](std::string const & feature) {
        std::cout << "Properties: Removing single feature: " << feature << std::endl;
        emit featureRemoveRequested(feature);
    });

    connect(ui->feature_tree_widget, &Feature_Tree_Widget::addFeatures, this, [this](std::vector<std::string> const & features) {
        std::cout << "Properties: Adding " << features.size() << " features as group" << std::endl;
        // Get colors for each feature from the model
        std::vector<std::string> colors;
        colors.reserve(features.size());
        for (auto const & key : features) {
            colors.push_back(_feature_tree_model->getFeatureColor(key));
        }
        emit featuresAddRequested(features, colors);
    });

    connect(ui->feature_tree_widget, &Feature_Tree_Widget::removeFeatures, this, [this](std::vector<std::string> const & features) {
        std::cout << "Properties: Removing " << features.size() << " features as group" << std::endl;
        emit featuresRemoveRequested(features);
    });

    // Connect color change signals from the model
    connect(_feature_tree_model.get(), &Feature_Tree_Model::featureColorChanged, 
            this, &DataViewerPropertiesWidget::_handleColorChanged);

    // Connect color change signals from the tree widget to the model
    connect(ui->feature_tree_widget, &Feature_Tree_Widget::colorChangeFeatures, this, [this](std::vector<std::string> const & features, std::string const & hex_color) {
        for (auto const & feature: features) {
            _feature_tree_model->setFeatureColor(feature, hex_color);
        }
    });
}

void DataViewerPropertiesWidget::_setupStackedWidget() {
    if (!_opengl_widget) {
        return;
    }

    // Remove all existing widgets from the stacked widget (except the placeholder page)
    while (ui->stackedWidget->count() > 1) {
        QWidget * widget = ui->stackedWidget->widget(ui->stackedWidget->count() - 1);
        ui->stackedWidget->removeWidget(widget);
        delete widget;
    }
    // Remove the placeholder page
    if (ui->stackedWidget->count() > 0) {
        QWidget * placeholder = ui->stackedWidget->widget(0);
        ui->stackedWidget->removeWidget(placeholder);
        delete placeholder;
    }

    // Setup stacked widget with data-type specific viewers
    auto * analog_widget = new AnalogViewer_Widget(_data_manager, _opengl_widget);
    auto * interval_widget = new IntervalViewer_Widget(_data_manager, _opengl_widget);
    auto * event_widget = new EventViewer_Widget(_data_manager, _opengl_widget);

    ui->stackedWidget->addWidget(analog_widget);   // Index 0
    ui->stackedWidget->addWidget(interval_widget); // Index 1
    ui->stackedWidget->addWidget(event_widget);    // Index 2

    // Connect color change signals from sub-widgets
    connect(analog_widget, &AnalogViewer_Widget::colorChanged,
            this, &DataViewerPropertiesWidget::_handleColorChanged);
    connect(interval_widget, &IntervalViewer_Widget::colorChanged,
            this, &DataViewerPropertiesWidget::_handleColorChanged);
    connect(event_widget, &EventViewer_Widget::colorChanged,
            this, &DataViewerPropertiesWidget::_handleColorChanged);
}
