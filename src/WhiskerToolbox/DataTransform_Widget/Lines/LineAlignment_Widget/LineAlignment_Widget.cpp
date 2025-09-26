#include "LineAlignment_Widget.hpp"

#include "ui_LineAlignment_Widget.h"

#include "DataManager.hpp"
#include "DataManager/transforms/Lines/Line_Alignment/line_alignment.hpp"
#include "Media/Media_Data.hpp"
#include <algorithm>

LineAlignment_Widget::LineAlignment_Widget(QWidget * parent)
    : DataManagerParameter_Widget(parent),
      ui(new Ui::LineAlignment_Widget) {
    ui->setupUi(this);

    // Setup approach combo box
    ui->approachComboBox->addItem("Peak Width Half Max", static_cast<int>(FWHMApproach::PEAK_WIDTH_HALF_MAX));
    ui->approachComboBox->addItem("Gaussian Fit", static_cast<int>(FWHMApproach::GAUSSIAN_FIT));
    ui->approachComboBox->addItem("Threshold Based", static_cast<int>(FWHMApproach::THRESHOLD_BASED));
    ui->approachComboBox->setCurrentIndex(0);

    // Setup output mode combo box
    ui->outputModeComboBox->addItem("Aligned Vertices", static_cast<int>(LineAlignmentOutputMode::ALIGNED_VERTICES));
    ui->outputModeComboBox->addItem("FWHM Profile Extents", static_cast<int>(LineAlignmentOutputMode::FWHM_PROFILE_EXTENTS));
    ui->outputModeComboBox->setCurrentIndex(0);

    // Set default values
    ui->widthSpinBox->setValue(20);
    ui->perpendicularRangeSpinBox->setValue(50);
    ui->useProcessedDataCheckBox->setChecked(true);
    ui->approachComboBox->setCurrentIndex(0);

    // Connect signals and slots
    connect(ui->widthSpinBox, QOverload<int>::of(&QSpinBox::valueChanged),
            this, &LineAlignment_Widget::_widthValueChanged);
    connect(ui->perpendicularRangeSpinBox, QOverload<int>::of(&QSpinBox::valueChanged),
            this, &LineAlignment_Widget::_perpendicularRangeValueChanged);
    connect(ui->useProcessedDataCheckBox, &QCheckBox::toggled,
            this, &LineAlignment_Widget::_useProcessedDataToggled);
    connect(ui->approachComboBox, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &LineAlignment_Widget::_approachChanged);
    connect(ui->outputModeComboBox, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &LineAlignment_Widget::_outputModeChanged);
    connect(ui->mediaDataKeyComboBox, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &LineAlignment_Widget::_mediaDataKeyChanged);
}

LineAlignment_Widget::~LineAlignment_Widget() {
    delete ui;
}

std::unique_ptr<TransformParametersBase> LineAlignment_Widget::getParameters() const {
    auto params = std::make_unique<LineAlignmentParameters>();

    // Use the selected media data key from the combo box
    auto dm = dataManager();
    if (dm && !_selected_media_key.empty()) {
        try {
            auto data_variant = dm->getDataVariant(_selected_media_key);
            if (data_variant.has_value() &&
                std::holds_alternative<std::shared_ptr<MediaData>>(*data_variant)) {
                params->media_data = std::get<std::shared_ptr<MediaData>>(*data_variant);
            } else {
                // If the selected key doesn't contain MediaData, return nullptr to indicate failure
                return nullptr;
            }
        } catch (...) {
            // If any exception occurs, return nullptr to indicate failure
            return nullptr;
        }
    } else {
        // No media data key selected, return nullptr to indicate failure
        return nullptr;
    }

    // Set other parameters from UI
    params->width = ui->widthSpinBox->value();
    params->perpendicular_range = ui->perpendicularRangeSpinBox->value();
    params->use_processed_data = ui->useProcessedDataCheckBox->isChecked();
    params->approach = static_cast<FWHMApproach>(ui->approachComboBox->currentData().toInt());
    params->output_mode = static_cast<LineAlignmentOutputMode>(ui->outputModeComboBox->currentData().toInt());

    return params;
}

void LineAlignment_Widget::onDataManagerChanged() {
    _refreshMediaDataKeys();
}

void LineAlignment_Widget::onDataManagerDataChanged() {
    _refreshMediaDataKeys();
}

void LineAlignment_Widget::_refreshMediaDataKeys() {
    auto dm = dataManager();
    if (!dm) {
        return;
    }

    // Get current media data keys
    auto media_keys = dm->getKeys<MediaData>();

    // Update the combo box
    _updateMediaDataKeyComboBox();

    // Enable/disable the combo box based on available keys
    bool has_keys = !media_keys.empty();
    ui->mediaDataKeyComboBox->setEnabled(has_keys);

    if (has_keys) {
        // Select the first key by default if none is currently selected
        if (_selected_media_key.empty() ||
            std::find(media_keys.begin(), media_keys.end(), _selected_media_key) == media_keys.end()) {
            _selected_media_key = media_keys[0];
            // Find the index of the selected key in the combo box
            int index = ui->mediaDataKeyComboBox->findText(QString::fromStdString(_selected_media_key));
            if (index >= 0) {
                ui->mediaDataKeyComboBox->setCurrentIndex(index);
            }
        }
    } else {
        // No keys available, clear selection
        _selected_media_key.clear();
        ui->mediaDataKeyComboBox->clear();
    }
}

void LineAlignment_Widget::_updateMediaDataKeyComboBox() {
    auto dm = dataManager();
    if (!dm) {
        return;
    }

    // Store current selection
    QString current_text = ui->mediaDataKeyComboBox->currentText();

    // Clear and repopulate the combo box
    ui->mediaDataKeyComboBox->clear();

    auto media_keys = dm->getKeys<MediaData>();
    for (auto const & key: media_keys) {
        ui->mediaDataKeyComboBox->addItem(QString::fromStdString(key));
    }

    // Restore selection if it still exists
    if (!current_text.isEmpty()) {
        int index = ui->mediaDataKeyComboBox->findText(current_text);
        if (index >= 0) {
            ui->mediaDataKeyComboBox->setCurrentIndex(index);
        }
    }
}

void LineAlignment_Widget::_mediaDataKeyChanged(int index) {
    auto dm = dataManager();
    if (index >= 0 && dm) {
        _selected_media_key = ui->mediaDataKeyComboBox->itemText(index).toStdString();
    }
}

void LineAlignment_Widget::_widthValueChanged(int value) {
    // Update the width label to show the current value
    ui->widthLabel->setText(QString("Width: %1 pixels").arg(value));
}

void LineAlignment_Widget::_perpendicularRangeValueChanged(int value) {
    // Update the perpendicular range label to show the current value
    ui->perpendicularRangeLabel->setText(QString("Perpendicular Range: %1 pixels").arg(value));
}

void LineAlignment_Widget::_useProcessedDataToggled(bool checked) {
    // Update the description based on the selection
    if (checked) {
        ui->dataTypeLabel->setText("Using processed data (filtered/enhanced images)");
    } else {
        ui->dataTypeLabel->setText("Using raw data (original images)");
    }
}

void LineAlignment_Widget::_approachChanged(int index) {
    // Update the approach parameter
    if (index >= 0) {
        FWHMApproach approach = static_cast<FWHMApproach>(ui->approachComboBox->itemData(index).toInt());
        (void)approach; // currently unused
    }
}

void LineAlignment_Widget::_outputModeChanged(int index) {
    // Update the output mode parameter
    if (index >= 0) {
        LineAlignmentOutputMode output_mode = static_cast<LineAlignmentOutputMode>(ui->outputModeComboBox->itemData(index).toInt());
        (void)output_mode; // currently unused
    }
}
