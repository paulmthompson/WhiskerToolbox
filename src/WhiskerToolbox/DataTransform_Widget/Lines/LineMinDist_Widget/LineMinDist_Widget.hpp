#ifndef LINEMINDIST_WIDGET_HPP
#define LINEMINDIST_WIDGET_HPP

#include "DataTransform_Widget/TransformParameter_Widget/TransformParameter_Widget.hpp"

namespace Ui { class LineMinDist_Widget; }
class DataManager;

class LineMinDist_Widget : public TransformParameter_Widget {
    Q_OBJECT
public:
    explicit LineMinDist_Widget(QWidget *parent = nullptr);
    ~LineMinDist_Widget() override;

    [[nodiscard]] std::unique_ptr<TransformParametersBase> getParameters() const override;
    void setDataManager(std::shared_ptr<DataManager> data_manager);

private:
    Ui::LineMinDist_Widget *ui;
    std::shared_ptr<DataManager> _data_manager;

private slots:
    void _pointFeatureSelected(QString const & feature);
};

#endif// LINEMINDIST_WIDGET_HPP
