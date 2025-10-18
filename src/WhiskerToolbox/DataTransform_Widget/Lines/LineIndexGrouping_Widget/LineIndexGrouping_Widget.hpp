#ifndef LINEINDEXGROUPING_WIDGET_HPP
#define LINEINDEXGROUPING_WIDGET_HPP

#include "DataTransform_Widget/TransformParameter_Widget/DataManagerParameter_Widget.hpp"

QT_BEGIN_NAMESPACE
class QLineEdit;
class QCheckBox;
class QLabel;
QT_END_NAMESPACE

namespace Ui {
class LineIndexGrouping_Widget;
}

class LineIndexGroupingParameters;

/**
 * @brief Widget for configuring line index grouping parameters
 * 
 * This widget provides user interface controls for setting up index-based
 * grouping of line data. Lines at the same vector index across different
 * timestamps are grouped together.
 */
class LineIndexGrouping_Widget : public DataManagerParameter_Widget {
    Q_OBJECT

public:
    explicit LineIndexGrouping_Widget(QWidget* parent = nullptr);
    ~LineIndexGrouping_Widget() override;

    [[nodiscard]] std::unique_ptr<TransformParametersBase> getParameters() const override;

protected:
    void onDataManagerChanged() override;

private slots:
    void onParametersChanged();
    void onClearExistingGroupsToggled(bool enabled);

private:
    Ui::LineIndexGrouping_Widget* ui;
    
    void setupUI();
    void connectSignals();
    void updateParametersFromUI();
};

#endif // LINEINDEXGROUPING_WIDGET_HPP
