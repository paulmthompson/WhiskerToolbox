#ifndef LINEPROXIMITYGROUPING_WIDGET_HPP
#define LINEPROXIMITYGROUPING_WIDGET_HPP

#include "DataTransform_Widget/TransformParameter_Widget/DataManagerParameter_Widget.hpp"

QT_BEGIN_NAMESPACE
class QDoubleSpinBox;
class QSpinBox;
class QCheckBox;
class QLineEdit;
class QPushButton;
class QLabel;
QT_END_NAMESPACE

namespace Ui {
class LineProximityGrouping_Widget;
}

class LineProximityGroupingParameters;

/**
 * @brief Widget for configuring line proximity grouping parameters
 * 
 * This widget provides user interface controls for setting up proximity-based
 * grouping of line data, including distance threshold, position along line,
 * and options for handling outliers.
 */
class LineProximityGrouping_Widget : public DataManagerParameter_Widget {
    Q_OBJECT

public:
    explicit LineProximityGrouping_Widget(QWidget* parent = nullptr);
    ~LineProximityGrouping_Widget() override;

    [[nodiscard]] std::unique_ptr<TransformParametersBase> getParameters() const override;

protected:
    void onDataManagerChanged() override;

private slots:
    void onParametersChanged();
    void onCreateNewGroupToggled(bool enabled);

private:
    Ui::LineProximityGrouping_Widget* ui;
    
    void setupUI();
    void connectSignals();
    void updateParametersFromUI();
    void updateNewGroupControls();
    
    // Helper to validate and update parameters
    bool validateParameters() const;
};

#endif // LINEPROXIMITYGROUPING_WIDGET_HPP