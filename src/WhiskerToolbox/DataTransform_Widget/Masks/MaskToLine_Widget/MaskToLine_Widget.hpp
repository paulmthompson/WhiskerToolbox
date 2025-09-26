#ifndef MASKTOLINE_WIDGET_HPP
#define MASKTOLINE_WIDGET_HPP

#include "DataTransform_Widget/TransformParameter_Widget/DataManagerParameter_Widget.hpp"


namespace Ui {
class MaskToLine_Widget;
}

class MaskToLine_Widget : public DataManagerParameter_Widget {
    Q_OBJECT
public:
    explicit MaskToLine_Widget(QWidget * parent = nullptr);
    ~MaskToLine_Widget() override;

    [[nodiscard]] std::unique_ptr<TransformParametersBase> getParameters() const override;

private slots:
    void onMethodChanged(int index);

private:
    Ui::MaskToLine_Widget * ui;
};

#endif// MASKTOLINE_WIDGET_HPP
