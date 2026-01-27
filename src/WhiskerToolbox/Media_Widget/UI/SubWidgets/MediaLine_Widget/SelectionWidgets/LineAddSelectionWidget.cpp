#include "LineAddSelectionWidget.hpp"
#include "ui_LineAddSelectionWidget.h"

#include <QButtonGroup>

namespace line_widget {

LineAddSelectionWidget::LineAddSelectionWidget(QWidget* parent)
    : QWidget(parent)
    , ui(new Ui::LineAddSelectionWidget) {
    ui->setupUi(this);
    _setupUI();
    _connectSignals();
}

LineAddSelectionWidget::~LineAddSelectionWidget() {
    delete ui;
}

void LineAddSelectionWidget::_setupUI() {
    // Create the button group for smoothing options
    _smoothingGroup = new QButtonGroup(this);
    _smoothingGroup->addButton(ui->simpleSmoothRadio, static_cast<int>(SmoothingMode::SimpleSmooth));
    _smoothingGroup->addButton(ui->polyFitRadio, static_cast<int>(SmoothingMode::PolynomialFit));
    
    // Store references to key UI elements for easier access
    _edgeParamsGroup = ui->edgeGroupBox;
    _thresholdSlider = ui->thresholdSlider;
    _radiusSpinBox = ui->radiusSpinBox;
    _polyOrderSpinBox = ui->polyOrderSpinBox;
}

void LineAddSelectionWidget::_connectSignals() {
    // Connect edge snapping checkbox
    connect(ui->edgeSnappingCheckbox, &QCheckBox::toggled, this, [this](bool checked) {
        _edgeSnappingEnabled = checked;
        ui->edgeGroupBox->setEnabled(checked);
        emit edgeSnappingToggled(checked);
    });
    
    // Connect smoothing mode radio buttons
    connect(_smoothingGroup, QOverload<int>::of(&QButtonGroup::idClicked), this, [this](int id) {
        _smoothingMode = static_cast<SmoothingMode>(id);
        emit smoothingModeChanged(id);
    });
    
    // Connect polynomial fit radio button to enable/disable polynomial order spinner
    connect(ui->polyFitRadio, &QRadioButton::toggled, ui->polyOrderSpinBox, &QSpinBox::setEnabled);
    
    // Connect polynomial order spinner
    connect(ui->polyOrderSpinBox, QOverload<int>::of(&QSpinBox::valueChanged), this, [this](int value) {
        _polynomialOrder = value;
        emit polynomialOrderChanged(value);
    });
    
    // Connect edge threshold slider
    connect(ui->thresholdSlider, &QSlider::valueChanged, this, [this](int value) {
        _edgeThreshold = value;
        ui->thresholdValueLabel->setText(QString::number(value));
        emit edgeThresholdChanged(value);
    });
    
    // Connect edge search radius spinner
    connect(ui->radiusSpinBox, QOverload<int>::of(&QSpinBox::valueChanged), this, [this](int value) {
        _edgeSearchRadius = value;
        emit edgeSearchRadiusChanged(value);
    });
}

bool LineAddSelectionWidget::isEdgeSnappingEnabled() const {
    return _edgeSnappingEnabled;
}

int LineAddSelectionWidget::getEdgeThreshold() const {
    return _edgeThreshold;
}

int LineAddSelectionWidget::getEdgeSearchRadius() const {
    return _edgeSearchRadius;
}

int LineAddSelectionWidget::getPolynomialOrder() const {
    return _polynomialOrder;
}

LineAddSelectionWidget::SmoothingMode LineAddSelectionWidget::getSmoothingMode() const {
    return _smoothingMode;
}

} // namespace line_widget 