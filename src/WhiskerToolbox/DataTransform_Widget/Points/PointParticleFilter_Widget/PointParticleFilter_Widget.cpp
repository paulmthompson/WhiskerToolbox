#include "PointParticleFilter_Widget.hpp"
#include "ui_PointParticleFilter_Widget.h"

#include "DataManager/DataManager.hpp"
#include "DataManager/Masks/Mask_Data.hpp"
#include "DataManager/transforms/Points/Point_Particle_Filter/point_particle_filter.hpp"

#include <QComboBox>
#include <QDoubleSpinBox>
#include <QLabel>
#include <QSpinBox>

PointParticleFilter_Widget::PointParticleFilter_Widget(QWidget * parent)
    : DataManagerParameter_Widget(parent),
      ui(new Ui::PointParticleFilter_Widget) {
    ui->setupUi(this);

    // Connect signals
    connect(ui->maskDataComboBox, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &PointParticleFilter_Widget::onMaskDataChanged);

    connect(ui->numParticlesSpinBox, QOverload<int>::of(&QSpinBox::valueChanged),
            this, &PointParticleFilter_Widget::updateInfoLabel);

    connect(ui->transitionRadiusSpinBox, QOverload<double>::of(&QDoubleSpinBox::valueChanged),
            this, &PointParticleFilter_Widget::updateInfoLabel);

    connect(ui->randomWalkProbSpinBox, QOverload<double>::of(&QDoubleSpinBox::valueChanged),
            this, &PointParticleFilter_Widget::updateInfoLabel);

    updateInfoLabel();
}

PointParticleFilter_Widget::~PointParticleFilter_Widget() {
    delete ui;
}

std::unique_ptr<TransformParametersBase> PointParticleFilter_Widget::getParameters() const {
    auto params = std::make_unique<PointParticleFilterParameters>();

    // Get mask data
    if (!_selectedMaskDataKey.isEmpty() && dataManager()) {
        auto mask_data_variant = dataManager()->getDataVariant(_selectedMaskDataKey.toStdString());
        if (mask_data_variant.has_value()) {
            if (auto mask_ptr = std::get_if<std::shared_ptr<MaskData>>(&mask_data_variant.value())) {
                params->mask_data = *mask_ptr;
            }
        }
    }

    // Get group manager from DataManager
    if (dataManager()) {
        params->group_manager = dataManager()->getEntityGroupManager();
    }

    // Get particle filter parameters from UI
    params->num_particles = static_cast<size_t>(ui->numParticlesSpinBox->value());
    params->transition_radius = static_cast<float>(ui->transitionRadiusSpinBox->value());
    params->random_walk_prob = static_cast<float>(ui->randomWalkProbSpinBox->value());

    return params;
}

void PointParticleFilter_Widget::setDataManager(std::shared_ptr<DataManager> dm) {
    DataManagerParameter_Widget::setDataManager(dm);
    populateMaskDataComboBox();
}

void PointParticleFilter_Widget::onDataManagerChanged() {
    populateMaskDataComboBox();
}

void PointParticleFilter_Widget::onDataManagerDataChanged() {
    populateMaskDataComboBox();
}

void PointParticleFilter_Widget::onMaskDataChanged(int index) {
    if (index >= 0 && index < ui->maskDataComboBox->count()) {
        _selectedMaskDataKey = ui->maskDataComboBox->itemData(index).toString();
        updateInfoLabel();
    } else {
        _selectedMaskDataKey.clear();
    }
}

void PointParticleFilter_Widget::populateMaskDataComboBox() {
    // Block signals while populating
    ui->maskDataComboBox->blockSignals(true);

    // Save current selection
    QString previousSelection = _selectedMaskDataKey;

    // Clear existing items
    ui->maskDataComboBox->clear();

    if (!dataManager()) {
        ui->maskDataComboBox->addItem("No DataManager", QVariant());
        ui->maskDataComboBox->setEnabled(false);
        ui->maskDataComboBox->blockSignals(false);
        return;
    }

    // Get all available data keys
    auto dm = dataManager();
    auto mask_keys = dm->getKeys<MaskData>();

    // Add items to combo box
    if (mask_keys.empty()) {
        ui->maskDataComboBox->addItem("No mask data available", QVariant());
        ui->maskDataComboBox->setEnabled(false);
    } else {
        ui->maskDataComboBox->setEnabled(true);
        ui->maskDataComboBox->clear();
        for (auto const & key: mask_keys) {
            ui->maskDataComboBox->addItem(QString::fromStdString(key),
                                          QString::fromStdString(key));
        }

        // Restore previous selection if it still exists
        int previousIndex = ui->maskDataComboBox->findData(previousSelection);
        if (previousIndex >= 0) {
            ui->maskDataComboBox->setCurrentIndex(previousIndex);
            _selectedMaskDataKey = previousSelection;
        } else if (ui->maskDataComboBox->count() > 0) {
            // Select first item by default
            ui->maskDataComboBox->setCurrentIndex(0);
            _selectedMaskDataKey = ui->maskDataComboBox->itemData(0).toString();
        }
    }

    ui->maskDataComboBox->blockSignals(false);
    updateInfoLabel();
}

void PointParticleFilter_Widget::updateInfoLabel() {
    QString infoText;

    if (_selectedMaskDataKey.isEmpty()) {
        infoText = "⚠️ Please select mask data. ";
    } else {
        infoText = QString("✓ Mask data: %1. ").arg(_selectedMaskDataKey);
    }

    infoText += QString("Using %1 particles with %2 pixel transition radius. ")
                        .arg(ui->numParticlesSpinBox->value())
                        .arg(ui->transitionRadiusSpinBox->value(), 0, 'f', 1);

    double rwProb = ui->randomWalkProbSpinBox->value();
    if (rwProb < 0.01) {
        infoText += "Purely local tracking.";
    } else if (rwProb > 0.5) {
        infoText += "High exploration mode.";
    } else {
        infoText += QString("%1% random exploration.").arg(rwProb * 100, 0, 'f', 1);
    }

    ui->infoLabel->setText(infoText);
}
