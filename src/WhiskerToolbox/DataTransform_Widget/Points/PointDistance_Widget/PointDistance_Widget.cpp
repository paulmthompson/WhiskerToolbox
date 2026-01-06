#include "PointDistance_Widget.hpp"

#include "ui_PointDistance_Widget.h"

#include "DataManager/DataManager.hpp"
#include "DataManager/transforms/Points/Point_Distance/point_distance.hpp"
#include "DataManager/Points/Point_Data.hpp"

PointDistance_Widget::PointDistance_Widget(QWidget *parent) :
      TransformParameter_Widget(parent),
      ui(new Ui::PointDistance_Widget),
      _data_manager(nullptr)
{
    ui->setupUi(this);

    // Set up reference type combo box
    ui->referenceTypeComboBox->addItem("Global Average", static_cast<int>(PointDistanceReferenceType::GlobalAverage));
    ui->referenceTypeComboBox->addItem("Rolling Average", static_cast<int>(PointDistanceReferenceType::RollingAverage));
    ui->referenceTypeComboBox->addItem("Set Point", static_cast<int>(PointDistanceReferenceType::SetPoint));
    ui->referenceTypeComboBox->addItem("Other Point Data", static_cast<int>(PointDistanceReferenceType::OtherPointData));

    // Set default window size
    ui->windowSizeSpinBox->setValue(1000);
    ui->windowSizeSpinBox->setMinimum(1);
    ui->windowSizeSpinBox->setMaximum(100000);

    // Set default coordinates
    ui->referenceXSpinBox->setValue(0.0);
    ui->referenceYSpinBox->setValue(0.0);
    ui->referenceXSpinBox->setMinimum(-100000.0);
    ui->referenceXSpinBox->setMaximum(100000.0);
    ui->referenceYSpinBox->setMinimum(-100000.0);
    ui->referenceYSpinBox->setMaximum(100000.0);

    // Connect signals
    connect(ui->referenceTypeComboBox, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &PointDistance_Widget::_onReferenceTypeChanged);

    // Initialize visibility
    _updateUIVisibility();
}

PointDistance_Widget::~PointDistance_Widget() {
    delete ui;
}

void PointDistance_Widget::setDataManager(std::shared_ptr<DataManager> data_manager) {
    _data_manager = std::move(data_manager);

    // Populate reference point data combo box with available point data
    ui->referencePointDataComboBox->clear();
    ui->referencePointDataComboBox->addItem("None", QString());

    if (_data_manager) {
        auto point_data_names = _data_manager->getKeys<PointData>();
        for (auto const & name : point_data_names) {
            ui->referencePointDataComboBox->addItem(QString::fromStdString(name), QString::fromStdString(name));
        }
    }
}

std::unique_ptr<TransformParametersBase> PointDistance_Widget::getParameters() const {
    auto params = std::make_unique<PointDistanceParameters>();

    // Get reference type
    int typeIndex = ui->referenceTypeComboBox->currentData().toInt();
    params->reference_type = static_cast<PointDistanceReferenceType>(typeIndex);

    // Get window size (for rolling average)
    params->window_size = ui->windowSizeSpinBox->value();

    // Get set point coordinates
    params->reference_x = static_cast<float>(ui->referenceXSpinBox->value());
    params->reference_y = static_cast<float>(ui->referenceYSpinBox->value());

    // Get reference point data
    if (params->reference_type == PointDistanceReferenceType::OtherPointData && _data_manager) {
        QString ref_name = ui->referencePointDataComboBox->currentData().toString();
        if (!ref_name.isEmpty()) {
            auto ref_point_data = _data_manager->getData<PointData>(ref_name.toStdString());
            params->reference_point_data = ref_point_data;
        }
    }

    return params;
}

void PointDistance_Widget::_onReferenceTypeChanged(int /*index*/) {
    _updateUIVisibility();
}

void PointDistance_Widget::_updateUIVisibility() {
    int typeIndex = ui->referenceTypeComboBox->currentData().toInt();
    auto refType = static_cast<PointDistanceReferenceType>(typeIndex);

    // Show/hide widgets based on reference type
    bool showWindowSize = (refType == PointDistanceReferenceType::RollingAverage);
    bool showSetPoint = (refType == PointDistanceReferenceType::SetPoint);
    bool showOtherPointData = (refType == PointDistanceReferenceType::OtherPointData);

    ui->windowSizeLabel->setVisible(showWindowSize);
    ui->windowSizeSpinBox->setVisible(showWindowSize);

    ui->referenceXLabel->setVisible(showSetPoint);
    ui->referenceXSpinBox->setVisible(showSetPoint);
    ui->referenceYLabel->setVisible(showSetPoint);
    ui->referenceYSpinBox->setVisible(showSetPoint);

    ui->referencePointDataLabel->setVisible(showOtherPointData);
    ui->referencePointDataComboBox->setVisible(showOtherPointData);
}
