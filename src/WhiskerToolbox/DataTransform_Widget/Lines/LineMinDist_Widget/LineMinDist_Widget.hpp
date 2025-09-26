#ifndef LINEMINDIST_WIDGET_HPP
#define LINEMINDIST_WIDGET_HPP

#include "DataTransform_Widget/TransformParameter_Widget/DataManagerParameter_Widget.hpp"

namespace Ui { class LineMinDist_Widget; }
class DataManager;

class LineMinDist_Widget : public DataManagerParameter_Widget {
    Q_OBJECT
public:
    explicit LineMinDist_Widget(QWidget *parent = nullptr);
    ~LineMinDist_Widget() override;

    [[nodiscard]] std::unique_ptr<TransformParametersBase> getParameters() const override;

protected:
    void onDataManagerChanged() override;
    void onDataManagerDataChanged() override;

private:
    Ui::LineMinDist_Widget *ui;

private slots:
    void _pointFeatureSelected(QString const & feature);
};

#endif// LINEMINDIST_WIDGET_HPP
