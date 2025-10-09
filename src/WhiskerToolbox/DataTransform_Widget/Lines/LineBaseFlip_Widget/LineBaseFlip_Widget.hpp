#ifndef LINEBASEFLIP_WIDGET_HPP
#define LINEBASEFLIP_WIDGET_HPP

#include "DataTransform_Widget/TransformParameter_Widget/DataManagerParameter_Widget.hpp"
#include "transforms/Lines/Line_Base_Flip/line_base_flip.hpp"

#include <QWidget>
#include <memory>

namespace Ui {
class LineBaseFlip_Widget;
}

/**
 * @brief Parameter widget for Line Base Flip transform
 *
 * This widget allows users to specify the reference point coordinates
 * that will be used to determine which end of each line should be the base.
 */
class LineBaseFlip_Widget : public DataManagerParameter_Widget {
    Q_OBJECT

public:
    explicit LineBaseFlip_Widget(QWidget* parent = nullptr);
    ~LineBaseFlip_Widget() override;

    std::unique_ptr<TransformParametersBase> getParameters() const override;

protected:
    void onDataManagerChanged() override;
    void onDataManagerDataChanged() override;

private slots:
    void onComboBoxSelectionChanged(int index);

private:
    void populateComboBoxWithPointData();
    void setSpinBoxesFromPointData(const QString& pointDataKey);

    Ui::LineBaseFlip_Widget* ui;
};

#endif // LINEBASEFLIP_WIDGET_HPP