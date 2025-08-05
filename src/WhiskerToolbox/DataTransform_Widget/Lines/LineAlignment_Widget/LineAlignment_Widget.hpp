#ifndef LINEALIGNMENT_WIDGET_HPP
#define LINEALIGNMENT_WIDGET_HPP

#include "DataTransform_Widget/TransformParameter_Widget/TransformParameter_Widget.hpp"
#include "DataManager/transforms/Lines/line_alignment.hpp"
#include "Feature_Table_Widget/Feature_Table_Widget.hpp"

#include <memory>

namespace Ui { class LineAlignment_Widget; }

class DataManager;

class LineAlignment_Widget : public TransformParameter_Widget {
    Q_OBJECT
public:
    explicit LineAlignment_Widget(QWidget *parent = nullptr);
    ~LineAlignment_Widget() override;

    [[nodiscard]] std::unique_ptr<TransformParametersBase> getParameters() const override;
    void setDataManager(std::shared_ptr<DataManager> data_manager);

private:
    Ui::LineAlignment_Widget *ui;
    std::shared_ptr<DataManager> _data_manager;

private slots:
    void _mediaFeatureSelected(QString const & feature);
    void _widthValueChanged(int value);
    void _useProcessedDataToggled(bool checked);
    void _approachChanged(int index);
};

#endif// LINEALIGNMENT_WIDGET_HPP 