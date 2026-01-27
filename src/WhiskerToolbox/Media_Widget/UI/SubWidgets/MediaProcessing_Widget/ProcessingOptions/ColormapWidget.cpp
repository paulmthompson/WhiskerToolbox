#include "ColormapWidget.hpp"
#include "ui_ColormapWidget.h"

#include <QCheckBox>
#include <QComboBox>
#include <QDoubleSpinBox>

ColormapWidget::ColormapWidget(QWidget* parent)
    : QWidget(parent),
      ui(new Ui::ColormapWidget) {
    
    ui->setupUi(this);

    // Populate the colormap combo box
    _populateColormapComboBox();

    // Connect UI controls to slots
    connect(ui->active_checkbox, &QCheckBox::toggled,
            this, &ColormapWidget::_onActiveChanged);
    connect(ui->colormap_combobox, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &ColormapWidget::_onColormapTypeChanged);
    connect(ui->alpha_spinbox, QOverload<double>::of(&QDoubleSpinBox::valueChanged),
            this, &ColormapWidget::_onAlphaChanged);
    connect(ui->normalize_checkbox, &QCheckBox::toggled,
            this, &ColormapWidget::_onNormalizeChanged);
}

ColormapWidget::~ColormapWidget() {
    delete ui;
}

ColormapOptions ColormapWidget::getOptions() const {
    ColormapOptions options;
    options.active = ui->active_checkbox->isChecked();
    options.colormap = _getColormapTypeFromIndex(ui->colormap_combobox->currentIndex());
    options.alpha = ui->alpha_spinbox->value();
    options.normalize = ui->normalize_checkbox->isChecked();
    return options;
}

void ColormapWidget::setOptions(ColormapOptions const& options) {
    _blockSignalsAndSetValues(options);
}

void ColormapWidget::setColormapEnabled(bool enabled) {
    setEnabled(enabled);
    if (!enabled) {
        // Disable colormap when not applicable (e.g., for color images)
        ui->active_checkbox->blockSignals(true);
        ui->active_checkbox->setChecked(false);
        ui->active_checkbox->blockSignals(false);
        _updateOptions();
    }
}

void ColormapWidget::_onActiveChanged() {
    _updateOptions();
}

void ColormapWidget::_onColormapTypeChanged() {
    // Auto-enable when user changes colormap type (unless it's "None")
    if (ui->colormap_combobox->currentIndex() > 0 && !ui->active_checkbox->isChecked()) {
        ui->active_checkbox->blockSignals(true);
        ui->active_checkbox->setChecked(true);
        ui->active_checkbox->blockSignals(false);
    }
    _updateOptions();
}

void ColormapWidget::_onAlphaChanged() {
    // Auto-enable when user changes values
    if (!ui->active_checkbox->isChecked()) {
        ui->active_checkbox->blockSignals(true);
        ui->active_checkbox->setChecked(true);
        ui->active_checkbox->blockSignals(false);
    }
    _updateOptions();
}

void ColormapWidget::_onNormalizeChanged() {
    _updateOptions();
}

void ColormapWidget::_updateOptions() {
    emit optionsChanged(getOptions());
}

void ColormapWidget::_blockSignalsAndSetValues(ColormapOptions const& options) {
    ui->active_checkbox->blockSignals(true);
    ui->colormap_combobox->blockSignals(true);
    ui->alpha_spinbox->blockSignals(true);
    ui->normalize_checkbox->blockSignals(true);

    ui->active_checkbox->setChecked(options.active);
    ui->colormap_combobox->setCurrentIndex(_getIndexFromColormapType(options.colormap));
    ui->alpha_spinbox->setValue(options.alpha);
    ui->normalize_checkbox->setChecked(options.normalize);

    ui->active_checkbox->blockSignals(false);
    ui->colormap_combobox->blockSignals(false);
    ui->alpha_spinbox->blockSignals(false);
    ui->normalize_checkbox->blockSignals(false);
}

void ColormapWidget::_populateColormapComboBox() {
    ui->colormap_combobox->addItem("None", static_cast<int>(ColormapType::None));

    // Add separator to distinguish single-color channel mappings
    ui->colormap_combobox->insertSeparator(ui->colormap_combobox->count());
    
    // Single-color channel mappings
    ui->colormap_combobox->addItem("Red Channel", static_cast<int>(ColormapType::Red));
    ui->colormap_combobox->addItem("Green Channel", static_cast<int>(ColormapType::Green));
    ui->colormap_combobox->addItem("Blue Channel", static_cast<int>(ColormapType::Blue));
    ui->colormap_combobox->addItem("Cyan Channel", static_cast<int>(ColormapType::Cyan));
    ui->colormap_combobox->addItem("Magenta Channel", static_cast<int>(ColormapType::Magenta));
    ui->colormap_combobox->addItem("Yellow Channel", static_cast<int>(ColormapType::Yellow));
    
    // Add separator to distinguish single-color channel mappings
    ui->colormap_combobox->insertSeparator(ui->colormap_combobox->count());

    // Standard OpenCV colormaps
    ui->colormap_combobox->addItem("Jet", static_cast<int>(ColormapType::Jet));
    ui->colormap_combobox->addItem("Hot", static_cast<int>(ColormapType::Hot));
    ui->colormap_combobox->addItem("Cool", static_cast<int>(ColormapType::Cool));
    ui->colormap_combobox->addItem("Spring", static_cast<int>(ColormapType::Spring));
    ui->colormap_combobox->addItem("Summer", static_cast<int>(ColormapType::Summer));
    ui->colormap_combobox->addItem("Autumn", static_cast<int>(ColormapType::Autumn));
    ui->colormap_combobox->addItem("Winter", static_cast<int>(ColormapType::Winter));
    ui->colormap_combobox->addItem("Rainbow", static_cast<int>(ColormapType::Rainbow));
    ui->colormap_combobox->addItem("Ocean", static_cast<int>(ColormapType::Ocean));
    ui->colormap_combobox->addItem("Pink", static_cast<int>(ColormapType::Pink));
    ui->colormap_combobox->addItem("HSV", static_cast<int>(ColormapType::HSV));
    ui->colormap_combobox->addItem("Parula", static_cast<int>(ColormapType::Parula));
    ui->colormap_combobox->addItem("Viridis", static_cast<int>(ColormapType::Viridis));
    ui->colormap_combobox->addItem("Plasma", static_cast<int>(ColormapType::Plasma));
    ui->colormap_combobox->addItem("Inferno", static_cast<int>(ColormapType::Inferno));
    ui->colormap_combobox->addItem("Magma", static_cast<int>(ColormapType::Magma));
    ui->colormap_combobox->addItem("Turbo", static_cast<int>(ColormapType::Turbo));

}

ColormapType ColormapWidget::_getColormapTypeFromIndex(int index) const {
    if (index >= 0 && index < ui->colormap_combobox->count()) {
        return static_cast<ColormapType>(ui->colormap_combobox->itemData(index).toInt());
    }
    return ColormapType::None;
}

int ColormapWidget::_getIndexFromColormapType(ColormapType type) const {
    for (int i = 0; i < ui->colormap_combobox->count(); ++i) {
        if (static_cast<ColormapType>(ui->colormap_combobox->itemData(i).toInt()) == type) {
            return i;
        }
    }
    return 0; // Default to "None"
}
