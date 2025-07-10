#include "SpatialOverlayPlotPropertiesWidget.hpp"
#include "../Plots/SpatialOverlayPlotWidget.hpp"

#include "DataManager/DataManager.hpp"
#include "DataManager/Points/Point_Data.hpp"

#include <QCheckBox>
#include <QDebug>
#include <QDoubleSpinBox>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QListWidget>
#include <QListWidgetItem>
#include <QPushButton>
#include <QSplitter>
#include <QVBoxLayout>

SpatialOverlayPlotPropertiesWidget::SpatialOverlayPlotPropertiesWidget(QWidget * parent)
    : AbstractPlotPropertiesWidget(parent),
      _spatial_plot_widget(nullptr) {

    initializeUI();
    setupConnections();
}

void SpatialOverlayPlotPropertiesWidget::setDataManager(std::shared_ptr<DataManager> data_manager) {
    qDebug() << "SpatialOverlayPlotPropertiesWidget: setDataManager called with DataManager:" << (data_manager != nullptr);
    _data_manager = std::move(data_manager);
}

void SpatialOverlayPlotPropertiesWidget::setPlotWidget(AbstractPlotWidget * plot_widget) {
    qDebug() << "SpatialOverlayPlotPropertiesWidget: setPlotWidget called with plot widget:" << (plot_widget != nullptr);

    _spatial_plot_widget = qobject_cast<SpatialOverlayPlotWidget *>(plot_widget);
    qDebug() << "SpatialOverlayPlotPropertiesWidget: Cast to SpatialOverlayPlotWidget:" << (_spatial_plot_widget != nullptr);

    if (_spatial_plot_widget) {
        qDebug() << "SpatialOverlayPlotPropertiesWidget: Updating available data sources and UI";
        // Update available data sources
        updateAvailableDataSources();
        
        // Update UI from plot
        updateFromPlot();
    }
}

void SpatialOverlayPlotPropertiesWidget::updateFromPlot() {
    qDebug() << "SpatialOverlayPlotPropertiesWidget: updateFromPlot called";
    if (_spatial_plot_widget) {
        // Update UI from current plot state
        QStringList current_keys = _spatial_plot_widget->getPointDataKeys();
        qDebug() << "SpatialOverlayPlotPropertiesWidget: updateFromPlot - current keys from plot:" << current_keys;
        setSelectedDataSources(current_keys);
        
        // Update zoom level and point size from current widget state
        if (_spatial_plot_widget->getOpenGLWidget()) {
            float current_zoom = _spatial_plot_widget->getOpenGLWidget()->getZoomLevel();
            float current_point_size = _spatial_plot_widget->getOpenGLWidget()->getPointSize();
            bool tooltips_enabled = _spatial_plot_widget->getOpenGLWidget()->getTooltipsEnabled();
            
            // Block signals to prevent recursive updates
            _zoom_level_spinbox->blockSignals(true);
            _point_size_spinbox->blockSignals(true);
            _tooltips_checkbox->blockSignals(true);
            
            _zoom_level_spinbox->setValue(static_cast<double>(current_zoom));
            _point_size_spinbox->setValue(static_cast<double>(current_point_size));
            _tooltips_checkbox->setChecked(tooltips_enabled);
            
            // Re-enable signals
            _zoom_level_spinbox->blockSignals(false);
            _point_size_spinbox->blockSignals(false);
            _tooltips_checkbox->blockSignals(false);
        }
    } else {
        qDebug() << "SpatialOverlayPlotPropertiesWidget: updateFromPlot - no spatial plot widget available";
    }
}

void SpatialOverlayPlotPropertiesWidget::applyToPlot() {
    updatePlotWidget();
}

void SpatialOverlayPlotPropertiesWidget::updateAvailableDataSources() {
    refreshDataSourcesList();
}

void SpatialOverlayPlotPropertiesWidget::onDataSourceItemChanged(QListWidgetItem * item) {
    qDebug() << "SpatialOverlayPlotPropertiesWidget: onDataSourceItemChanged called for item:" << item->text() 
             << "checked:" << (item->checkState() == Qt::Checked);
    updatePlotWidget();
}

void SpatialOverlayPlotPropertiesWidget::onPointSizeChanged(double value) {
    qDebug() << "SpatialOverlayPlotPropertiesWidget: onPointSizeChanged called with value:" << value;
    if (_spatial_plot_widget && _spatial_plot_widget->getOpenGLWidget()) {
        _spatial_plot_widget->getOpenGLWidget()->setPointSize(static_cast<float>(value));
    }
}

void SpatialOverlayPlotPropertiesWidget::onZoomLevelChanged(double value) {
    if (_spatial_plot_widget && _spatial_plot_widget->getOpenGLWidget()) {
        _spatial_plot_widget->getOpenGLWidget()->setZoomLevel(static_cast<float>(value));
    }
}

void SpatialOverlayPlotPropertiesWidget::onResetViewClicked() {
    if (_spatial_plot_widget && _spatial_plot_widget->getOpenGLWidget()) {
        // Reset zoom and pan to defaults
        _spatial_plot_widget->getOpenGLWidget()->setZoomLevel(1.0f);
        _spatial_plot_widget->getOpenGLWidget()->setPanOffset(0.0f, 0.0f);
        _zoom_level_spinbox->setValue(1.0);
    }
}

void SpatialOverlayPlotPropertiesWidget::onTooltipsEnabledChanged(bool enabled) {
    qDebug() << "SpatialOverlayPlotPropertiesWidget: onTooltipsEnabledChanged called with enabled:" << enabled;
    if (_spatial_plot_widget && _spatial_plot_widget->getOpenGLWidget()) {
        _spatial_plot_widget->getOpenGLWidget()->setTooltipsEnabled(enabled);
    }
}

void SpatialOverlayPlotPropertiesWidget::onSelectAllClicked() {
    for (int i = 0; i < _data_sources_list->count(); ++i) {
        QListWidgetItem * item = _data_sources_list->item(i);
        item->setCheckState(Qt::Checked);
    }
    updatePlotWidget();
}

void SpatialOverlayPlotPropertiesWidget::onDeselectAllClicked() {
    for (int i = 0; i < _data_sources_list->count(); ++i) {
        QListWidgetItem * item = _data_sources_list->item(i);
        item->setCheckState(Qt::Unchecked);
    }
    updatePlotWidget();
}

void SpatialOverlayPlotPropertiesWidget::initializeUI() {
    QVBoxLayout * main_layout = new QVBoxLayout(this);
    main_layout->setContentsMargins(5, 5, 5, 5);
    main_layout->setSpacing(10);

    // Data Sources Group
    _data_sources_group = new QGroupBox("Point Data Sources", this);
    QVBoxLayout * sources_layout = new QVBoxLayout(_data_sources_group);

    // Selection buttons
    QHBoxLayout * selection_buttons_layout = new QHBoxLayout();
    _select_all_button = new QPushButton("Select All", this);
    _deselect_all_button = new QPushButton("Deselect All", this);
    _select_all_button->setMaximumWidth(80);
    _deselect_all_button->setMaximumWidth(80);

    selection_buttons_layout->addWidget(_select_all_button);
    selection_buttons_layout->addWidget(_deselect_all_button);
    selection_buttons_layout->addStretch();

    sources_layout->addLayout(selection_buttons_layout);

    // Data sources list
    _data_sources_list = new QListWidget(this);
    _data_sources_list->setMaximumHeight(150);
    sources_layout->addWidget(_data_sources_list);

    main_layout->addWidget(_data_sources_group);

    // Visualization Settings Group
    _visualization_group = new QGroupBox("Visualization Settings", this);
    QVBoxLayout * vis_layout = new QVBoxLayout(_visualization_group);

    // Point size setting
    QHBoxLayout * point_size_layout = new QHBoxLayout();
    point_size_layout->addWidget(new QLabel("Point Size:", this));
    _point_size_spinbox = new QDoubleSpinBox(this);
    _point_size_spinbox->setRange(1.0, 50.0);
    _point_size_spinbox->setSingleStep(0.5);
    _point_size_spinbox->setValue(8.0); // Match default in OpenGL widget
    _point_size_spinbox->setSuffix(" px");
    point_size_layout->addWidget(_point_size_spinbox);
    point_size_layout->addStretch();
    vis_layout->addLayout(point_size_layout);

    // Zoom level setting
    QHBoxLayout * zoom_layout = new QHBoxLayout();
    zoom_layout->addWidget(new QLabel("Zoom Level:", this));
    _zoom_level_spinbox = new QDoubleSpinBox(this);
    _zoom_level_spinbox->setRange(0.1, 10.0);
    _zoom_level_spinbox->setSingleStep(0.1);
    _zoom_level_spinbox->setValue(1.0);
    _zoom_level_spinbox->setSuffix("x");
    zoom_layout->addWidget(_zoom_level_spinbox);
    zoom_layout->addStretch();
    vis_layout->addLayout(zoom_layout);

    // Reset view button
    _reset_view_button = new QPushButton("Reset View", this);
    _reset_view_button->setMaximumWidth(100);
    vis_layout->addWidget(_reset_view_button);

    // Tooltips checkbox
    _tooltips_checkbox = new QCheckBox("Show Tooltips", this);
    _tooltips_checkbox->setChecked(true); // Default to enabled
    vis_layout->addWidget(_tooltips_checkbox);

    main_layout->addWidget(_visualization_group);

    // Add stretch to push everything to the top
    main_layout->addStretch();
}

void SpatialOverlayPlotPropertiesWidget::setupConnections() {
    qDebug() << "SpatialOverlayPlotPropertiesWidget: setupConnections called";
    
    // Data source selection
    connect(_data_sources_list, &QListWidget::itemChanged,
            this, &SpatialOverlayPlotPropertiesWidget::onDataSourceItemChanged);

    connect(_select_all_button, &QPushButton::clicked,
            this, &SpatialOverlayPlotPropertiesWidget::onSelectAllClicked);

    connect(_deselect_all_button, &QPushButton::clicked,
            this, &SpatialOverlayPlotPropertiesWidget::onDeselectAllClicked);

    // Visualization settings
    connect(_point_size_spinbox, QOverload<double>::of(&QDoubleSpinBox::valueChanged),
            this, &SpatialOverlayPlotPropertiesWidget::onPointSizeChanged);

    connect(_zoom_level_spinbox, QOverload<double>::of(&QDoubleSpinBox::valueChanged),
            this, &SpatialOverlayPlotPropertiesWidget::onZoomLevelChanged);

    connect(_reset_view_button, &QPushButton::clicked,
            this, &SpatialOverlayPlotPropertiesWidget::onResetViewClicked);

    // Tooltips checkbox
    connect(_tooltips_checkbox, &QCheckBox::toggled,
            this, &SpatialOverlayPlotPropertiesWidget::onTooltipsEnabledChanged);
    
    qDebug() << "SpatialOverlayPlotPropertiesWidget: setupConnections completed";
}

void SpatialOverlayPlotPropertiesWidget::refreshDataSourcesList() {
    _data_sources_list->clear();

    if (!_data_manager) {
        qDebug() << "SpatialOverlayPlotPropertiesWidget: No data manager available";
        return;
    }

    // Get all PointData objects from DataManager
    auto point_data_keys = _data_manager->getKeys<PointData>();
    qDebug() << "SpatialOverlayPlotPropertiesWidget: Found" << point_data_keys.size() << "PointData keys";

    for (auto const & key: point_data_keys) {
        qDebug() << "SpatialOverlayPlotPropertiesWidget: Adding data source:" << QString::fromStdString(key);
        QListWidgetItem * item = new QListWidgetItem(QString::fromStdString(key), _data_sources_list);
        item->setFlags(item->flags() | Qt::ItemIsUserCheckable);
        item->setCheckState(Qt::Unchecked);
        _data_sources_list->addItem(item);
    }
}

QStringList SpatialOverlayPlotPropertiesWidget::getSelectedDataSources() const {
    QStringList selected_keys;

    for (int i = 0; i < _data_sources_list->count(); ++i) {
        QListWidgetItem * item = _data_sources_list->item(i);
        if (item->checkState() == Qt::Checked) {
            selected_keys.append(item->text());
        }
    }

    return selected_keys;
}

void SpatialOverlayPlotPropertiesWidget::setSelectedDataSources(QStringList const & selected_keys) {
    qDebug() << "SpatialOverlayPlotPropertiesWidget: setSelectedDataSources called with keys:" << selected_keys;
    qDebug() << "SpatialOverlayPlotPropertiesWidget: List widget has" << _data_sources_list->count() << "items";
    
    // Block signals to prevent recursion when setting checkbox states
    _data_sources_list->blockSignals(true);
    
    for (int i = 0; i < _data_sources_list->count(); ++i) {
        QListWidgetItem * item = _data_sources_list->item(i);
        bool should_be_checked = selected_keys.contains(item->text());
        qDebug() << "SpatialOverlayPlotPropertiesWidget: Setting item" << item->text() << "to checked:" << should_be_checked;
        
        if (should_be_checked) {
            item->setCheckState(Qt::Checked);
        } else {
            item->setCheckState(Qt::Unchecked);
        }
    }
    
    // Re-enable signals
    _data_sources_list->blockSignals(false);
}

void SpatialOverlayPlotPropertiesWidget::updatePlotWidget() {
    if (!_spatial_plot_widget) {
        qDebug() << "SpatialOverlayPlotPropertiesWidget: No spatial plot widget available";
        return;
    }

    // Update selected data sources
    QStringList selected_keys = getSelectedDataSources();
    qDebug() << "SpatialOverlayPlotPropertiesWidget: Updating plot with selected keys:" << selected_keys;
    _spatial_plot_widget->setPointDataKeys(selected_keys);

    // Update visualization settings
    // Note: We'll need to add public interfaces to SpatialOverlayPlotWidget for these
    // For now, the basic data source update is working
}