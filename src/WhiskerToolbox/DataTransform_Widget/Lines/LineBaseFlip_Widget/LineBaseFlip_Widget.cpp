#include "LineBaseFlip_Widget.hpp"
#include "ui_LineBaseFlip_Widget.h"

#include "DataManager/DataManager.hpp"
#include "DataManager/Points/Point_Data.hpp"
#include "transforms/Lines/Line_Base_Flip/line_base_flip.hpp"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGridLayout>
#include <QLabel>
#include <QGroupBox>
#include <QDoubleSpinBox>
#include <QComboBox>
#include <QString>
#include <QShowEvent>
#include <QTimer>
#include <QDebug>



LineBaseFlip_Widget::LineBaseFlip_Widget(QWidget* parent)
    : DataManagerParameter_Widget(parent)
    , ui(new Ui::LineBaseFlip_Widget)
{
    ui->setupUi(this);

    // Connect combo box signal
    connect(ui->comboBox, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &LineBaseFlip_Widget::onComboBoxSelectionChanged);

    // Initialize combo box (may be empty if DataManager not set yet)
    populateComboBoxWithPointData();

    // Set up a timer to periodically refresh the combo box as a fallback
    auto* refreshTimer = new QTimer(this);
    connect(refreshTimer, &QTimer::timeout, this, &LineBaseFlip_Widget::populateComboBoxWithPointData);
    refreshTimer->start(2000); // Refresh every 2 seconds
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

void LineBaseFlip_Widget::showEvent(QShowEvent* event) {
    DataManagerParameter_Widget::showEvent(event);
    // Refresh combo box when widget becomes visible to catch any new point data
    populateComboBoxWithPointData();
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
    // Store current selection to restore it if possible
    QString currentSelection = ui->comboBox->currentText();

    ui->comboBox->clear();

    // Add default "None" option
    ui->comboBox->addItem("(None)");

    // Check if we have a data manager
    auto dm = dataManager();
    if (!dm) {
        qDebug() << "LineBaseFlip_Widget: No DataManager available";
        return;
    }

    // Get all PointData keys and add them to combo box
    auto pointDataKeys = dm->getKeys<PointData>();
    qDebug() << "LineBaseFlip_Widget: Found" << pointDataKeys.size() << "PointData keys";

    for (const auto& key : pointDataKeys) {
        qDebug() << "LineBaseFlip_Widget: Adding PointData key:" << QString::fromStdString(key);
        ui->comboBox->addItem(QString::fromStdString(key));
    }

    // Try to restore previous selection if it still exists
    if (!currentSelection.isEmpty() && currentSelection != "(None)") {
        int index = ui->comboBox->findText(currentSelection);
        if (index != -1) {
            ui->comboBox->setCurrentIndex(index);
        }
    }

    qDebug() << "LineBaseFlip_Widget: Combo box now has" << ui->comboBox->count() << "items";
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

    // Take the first point and set coordinates correctly (no swapping)
    const auto& firstPoint = points.front();
    ui->xSpinBox->setValue(static_cast<double>(firstPoint.x)); // X spinbox gets X coordinate
    ui->ySpinBox->setValue(static_cast<double>(firstPoint.y)); // Y spinbox gets Y coordinate
}
