#include "LineBaseFlip_Widget.hpp"
#include "ui_LineBaseFlip_Widget.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGridLayout>
#include <QLabel>
#include <QGroupBox>
#include <QDoubleSpinBox>
#include <QComboBox>
#include <QString>

// Include DataManager and PointData headers
#include "DataManager/DataManager.hpp"
#include "DataManager/Points/Point_Data.hpp"

LineBaseFlip_Widget::LineBaseFlip_Widget(QWidget* parent)
    : DataManagerParameter_Widget(parent)
    , ui(new Ui::LineBaseFlip_Widget)
{
    ui->setupUi(this);

    // Connect combo box signal
    connect(ui->comboBox, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &LineBaseFlip_Widget::onComboBoxSelectionChanged);

    // Initialize combo box
    populateComboBoxWithPointData();
}

LineBaseFlip_Widget::~LineBaseFlip_Widget()
{
    delete ui;
}

std::unique_ptr<TransformParametersBase> LineBaseFlip_Widget::getParameters() const {
    Point2D<float> reference_point{
        static_cast<float>(ui->xSpinBox->value()),
        static_cast<float>(ui->ySpinBox->value())
    };

    return std::make_unique<LineBaseFlipParameters>(reference_point);
}

void LineBaseFlip_Widget::onDataManagerChanged() {
    // Repopulate combo box when data manager changes
    populateComboBoxWithPointData();
}

void LineBaseFlip_Widget::onDataManagerDataChanged() {
    // Repopulate combo box when data changes
    populateComboBoxWithPointData();
}

void LineBaseFlip_Widget::populateComboBoxWithPointData() {
    ui->comboBox->clear();

    // Add default "None" option
    ui->comboBox->addItem("(None)");

    // Check if we have a data manager
    auto dm = dataManager();
    if (!dm) {
        return;
    }

    // Get all PointData keys and add them to combo box
    auto pointDataKeys = dm->getKeys<PointData>();
    for (const auto& key : pointDataKeys) {
        ui->comboBox->addItem(QString::fromStdString(key));
    }
}

void LineBaseFlip_Widget::onComboBoxSelectionChanged(int index) {
    if (index <= 0) {
        // "(None)" selected or invalid index
        return;
    }

    QString selectedKey = ui->comboBox->itemText(index);
    setSpinBoxesFromPointData(selectedKey);
}

void LineBaseFlip_Widget::setSpinBoxesFromPointData(const QString& pointDataKey) {
    auto dm = dataManager();
    if (!dm) {
        return;
    }

    std::string key = pointDataKey.toStdString();
    auto pointData = dm->getData<PointData>(key);
    if (!pointData) {
        return;
    }

    // Get the first available time with data
    auto timesWithData = pointData->getTimesWithData();
    if (timesWithData.empty()) {
        return;
    }

    // Get points at the first time
    auto firstTime = *timesWithData.begin();
    const auto& points = pointData->getAtTime(firstTime);
    if (points.empty()) {
        return;
    }

    // Take the first point and swap X and Y as requested
    const auto& firstPoint = points.front();
    ui->xSpinBox->setValue(static_cast<double>(firstPoint.y)); // X spinbox gets Y coordinate
    ui->ySpinBox->setValue(static_cast<double>(firstPoint.x)); // Y spinbox gets X coordinate
}
