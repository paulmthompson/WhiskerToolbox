#ifndef LINEEXTEND_WIDGET_HPP
#define LINEEXTEND_WIDGET_HPP

#include "DataTransform_Widget/TransformParameter_Widget/DataManagerParameter_Widget.hpp"

namespace Ui {
class LineExtend_Widget;
}

class LineExtend_Widget : public DataManagerParameter_Widget {
    Q_OBJECT
public:
    explicit LineExtend_Widget(QWidget * parent = nullptr);
    ~LineExtend_Widget() override;

    [[nodiscard]] std::unique_ptr<TransformParametersBase> getParameters() const override;

protected:
    void onDataManagerChanged() override;
    void onDataManagerDataChanged() override;

private slots:
    void _lineFeatureSelected(QString const & feature);
    void onExtendEndChanged(int index);

private:
    Ui::LineExtend_Widget * ui;
};

#endif// LINEEXTEND_WIDGET_HPP
