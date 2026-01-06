#ifndef POINTDISTANCE_WIDGET_HPP
#define POINTDISTANCE_WIDGET_HPP

#include "DataTransform_Widget/TransformParameter_Widget/TransformParameter_Widget.hpp"

#include <memory>

class DataManager;

namespace Ui { class PointDistance_Widget; }

class PointDistance_Widget : public TransformParameter_Widget {
    Q_OBJECT
public:
    explicit PointDistance_Widget(QWidget *parent = nullptr);
    ~PointDistance_Widget() override;

    [[nodiscard]] std::unique_ptr<TransformParametersBase> getParameters() const override;

    void setDataManager(std::shared_ptr<DataManager> data_manager);

private slots:
    void _onReferenceTypeChanged(int index);

private:
    Ui::PointDistance_Widget *ui;
    std::shared_ptr<DataManager> _data_manager;

    void _updateUIVisibility();
};

#endif// POINTDISTANCE_WIDGET_HPP

