#include "EventPlotPropertiesWidget.hpp"
#include "EventPlotWidget.hpp"
#include "EventPlotOpenGLWidget.hpp"

#include "ui_EventPlotPropertiesWidget.h"

#include <QCheckBox>
#include <QDoubleSpinBox>
#include <QListWidget>
#include <QListWidgetItem>
#include <QMessageBox>
#include <QPushButton>

EventPlotPropertiesWidget::EventPlotPropertiesWidget(QWidget * parent)
    : AbstractPlotPropertiesWidget(parent),
      ui(new Ui::EventPlotPropertiesWidget),
      _event_plot_widget(nullptr) {
    ui->setupUi(this);
    setupConnections();
}

EventPlotPropertiesWidget::~EventPlotPropertiesWidget() {
    delete ui;
}

void EventPlotPropertiesWidget::setDataManager(std::shared_ptr<DataManager> data_manager) {
    _data_manager = data_manager;
    updateAvailableDataSources();
}

void EventPlotPropertiesWidget::setPlotWidget(AbstractPlotWidget * plot_widget) {
    _plot_widget = plot_widget;
    _event_plot_widget = qobject_cast<EventPlotWidget *>(plot_widget);

    if (_event_plot_widget) {
        updateFromPlot();
    }
}

void EventPlotPropertiesWidget::updateFromPlot() {
    if (!_event_plot_widget) {
        return;
    }

    // Update UI with current plot settings
    setSelectedEventDataSources(_event_plot_widget->getEventDataKeys());

    // Update zoom level
    if (_event_plot_widget->getOpenGLWidget()) {
        float current_zoom = _event_plot_widget->getOpenGLWidget()->getZoomLevel();
        ui->zoom_level_spinbox->blockSignals(true);
        ui->zoom_level_spinbox->setValue(static_cast<double>(current_zoom));
        ui->zoom_level_spinbox->blockSignals(false);

        bool tooltips_enabled = _event_plot_widget->getOpenGLWidget()->getTooltipsEnabled();
        ui->tooltips_checkbox->blockSignals(true);
        ui->tooltips_checkbox->setChecked(tooltips_enabled);
        ui->tooltips_checkbox->blockSignals(false);
    }
}

void EventPlotPropertiesWidget::applyToPlot() {
    if (!_event_plot_widget) {
        return;
    }

    // Apply current settings to the plot widget
    updatePlotWidget();
}

void EventPlotPropertiesWidget::updateAvailableDataSources() {
    // TODO: Populate available event data sources from DataManager
    // This will be implemented when we have access to DataManager event data
    if (ui->event_data_sources_list) {
        ui->event_data_sources_list->clear();
        // TODO: Add event data sources from DataManager
    }
}

void EventPlotPropertiesWidget::onEventDataSourcesChanged() {
    // Update plot widget when event data sources change
    updatePlotWidget();
}

void EventPlotPropertiesWidget::onZoomLevelChanged(double value) {
    if (_event_plot_widget && _event_plot_widget->getOpenGLWidget()) {
        _event_plot_widget->getOpenGLWidget()->setZoomLevel(static_cast<float>(value));
    }
}

void EventPlotPropertiesWidget::onResetViewClicked() {
    if (_event_plot_widget && _event_plot_widget->getOpenGLWidget()) {
        _event_plot_widget->getOpenGLWidget()->setZoomLevel(1.0f);
        _event_plot_widget->getOpenGLWidget()->setPanOffset(0.0f, 0.0f);
    }
}

void EventPlotPropertiesWidget::onTooltipsEnabledChanged(bool enabled) {
    if (_event_plot_widget && _event_plot_widget->getOpenGLWidget()) {
        _event_plot_widget->getOpenGLWidget()->setTooltipsEnabled(enabled);
    }
}

void EventPlotPropertiesWidget::setupConnections() {
    // Connect UI signals to slots
    if (ui->event_data_sources_list) {
        connect(ui->event_data_sources_list, &QListWidget::itemSelectionChanged,
                this, &EventPlotPropertiesWidget::onEventDataSourcesChanged);
    }

    if (ui->zoom_level_spinbox) {
        connect(ui->zoom_level_spinbox, QOverload<double>::of(&QDoubleSpinBox::valueChanged),
                this, &EventPlotPropertiesWidget::onZoomLevelChanged);
    }

    if (ui->reset_view_button) {
        connect(ui->reset_view_button, &QPushButton::clicked,
                this, &EventPlotPropertiesWidget::onResetViewClicked);
    }

    if (ui->tooltips_checkbox) {
        connect(ui->tooltips_checkbox, &QCheckBox::toggled,
                this, &EventPlotPropertiesWidget::onTooltipsEnabledChanged);
    }
}

QStringList EventPlotPropertiesWidget::getSelectedEventDataSources() const {
    QStringList selected_sources;
    if (ui->event_data_sources_list) {
        QList<QListWidgetItem *> selected_items = ui->event_data_sources_list->selectedItems();
        for (QListWidgetItem * item: selected_items) {
            selected_sources.append(item->text());
        }
    }
    return selected_sources;
}

void EventPlotPropertiesWidget::setSelectedEventDataSources(QStringList const & selected_keys) {
    if (ui->event_data_sources_list) {
        ui->event_data_sources_list->clearSelection();
        for (QString const & key: selected_keys) {
            QList<QListWidgetItem *> items = ui->event_data_sources_list->findItems(key, Qt::MatchExactly);
            for (QListWidgetItem * item: items) {
                item->setSelected(true);
            }
        }
    }
}

void EventPlotPropertiesWidget::updatePlotWidget() {
    if (!_event_plot_widget) {
        return;
    }

    // Update event data keys
    QStringList selected_sources = getSelectedEventDataSources();
    _event_plot_widget->setEventDataKeys(selected_sources);

    // Emit properties changed signal
    emit propertiesChanged();
}