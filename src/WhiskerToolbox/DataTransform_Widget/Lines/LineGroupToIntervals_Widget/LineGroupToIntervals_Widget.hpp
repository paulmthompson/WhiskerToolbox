#ifndef LINEGROUPTOINTERVALS_WIDGET_HPP
#define LINEGROUPTOINTERVALS_WIDGET_HPP

#include "DataTransform_Widget/TransformParameter_Widget/DataManagerParameter_Widget.hpp"

QT_BEGIN_NAMESPACE
class QComboBox;
class QCheckBox;
class QSpinBox;
class QLabel;
QT_END_NAMESPACE

namespace Ui {
class LineGroupToIntervals_Widget;
}

class LineGroupToIntervalsParameters;

/**
 * @brief Widget for configuring line group to intervals parameters
 * 
 * This widget provides user interface controls for converting line group
 * presence/absence into digital interval series. Users can select a target
 * group, choose to track presence or absence, and configure filtering options.
 */
class LineGroupToIntervals_Widget : public DataManagerParameter_Widget {
    Q_OBJECT

public:
    explicit LineGroupToIntervals_Widget(QWidget* parent = nullptr);
    ~LineGroupToIntervals_Widget() override;

    [[nodiscard]] std::unique_ptr<TransformParametersBase> getParameters() const override;

protected:
    void onDataManagerChanged() override;
    void onDataManagerDataChanged() override;

private slots:
    void onParametersChanged();
    void onTrackPresenceToggled(bool trackPresence);
    void onGroupSelected(int index);
    void refreshGroupList();

private:
    Ui::LineGroupToIntervals_Widget* ui;
    
    void setupUI();
    void connectSignals();
    void updateParametersFromUI();
    void populateGroupComboBox();
    void updateInfoText();
};

#endif // LINEGROUPTOINTERVALS_WIDGET_HPP
