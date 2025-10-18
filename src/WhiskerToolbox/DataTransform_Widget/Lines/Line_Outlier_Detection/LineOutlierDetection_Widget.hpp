#ifndef LINEOUTLIERDETECTION_WIDGET_HPP
#define LINEOUTLIERDETECTION_WIDGET_HPP

#include "DataTransform_Widget/TransformParameter_Widget/DataManagerParameter_Widget.hpp"

#include <memory>

QT_BEGIN_NAMESPACE
class QDoubleSpinBox;
class QCheckBox;
class QLineEdit;
class QPushButton;
QT_END_NAMESPACE

namespace Ui {
class LineOutlierDetection_Widget;
}

class LineOutlierDetectionParameters;

/**
 * @brief Widget for configuring line outlier detection parameters
 * 
 * This widget provides user interface controls for setting up Kalman filter-based
 * outlier detection for line data, including noise parameters, MAD threshold,
 * and output control options.
 */
class LineOutlierDetection_Widget : public DataManagerParameter_Widget {
    Q_OBJECT

public:
    explicit LineOutlierDetection_Widget(QWidget * parent = nullptr);
    ~LineOutlierDetection_Widget() override;

    [[nodiscard]] std::unique_ptr<TransformParametersBase> getParameters() const override;

protected:
    void onDataManagerChanged() override;

private slots:
    void onParametersChanged();

private:
    std::unique_ptr<Ui::LineOutlierDetection_Widget> ui;

    void setupUI();
    void connectSignals();
    void loadDefaultValues();
    void updateParametersFromUI();
    void resetToDefaults();

    bool validateParameters() const;
};

#endif // LINEOUTLIERDETECTION_WIDGET_HPP
