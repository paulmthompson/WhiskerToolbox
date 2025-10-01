#ifndef LINEKALMANGROUOING_WIDGET_HPP
#define LINEKALMANGROUOING_WIDGET_HPP

#include "DataTransform_Widget/TransformParameter_Widget/DataManagerParameter_Widget.hpp"

#include <memory>

QT_BEGIN_NAMESPACE
class QDoubleSpinBox;
class QSpinBox;
class QCheckBox;
class QLineEdit;
class QPushButton;
class QLabel;
class QGroupBox;
QT_END_NAMESPACE

namespace Ui {
class LineKalmanGrouping_Widget;
}

class LineKalmanGroupingParameters;

/**
 * @brief Widget for configuring line Kalman grouping parameters
 * 
 * This widget provides user interface controls for setting up Kalman filter-based
 * tracking and grouping of line data, including noise parameters, assignment thresholds,
 * and algorithm control options.
 */
class LineKalmanGrouping_Widget : public DataManagerParameter_Widget {
    Q_OBJECT

public:
    explicit LineKalmanGrouping_Widget(QWidget* parent = nullptr);
    ~LineKalmanGrouping_Widget() override;

    [[nodiscard]] std::unique_ptr<TransformParametersBase> getParameters() const override;

protected:
    void onDataManagerChanged() override;

private slots:
    void onParametersChanged();

private:
    std::unique_ptr<Ui::LineKalmanGrouping_Widget> ui;
    
    void setupUI();
    void connectSignals();
    void updateParametersFromUI();
    void resetToDefaults();
    
    bool validateParameters() const;
};

#endif // LINEKALMANGROUOING_WIDGET_HPP