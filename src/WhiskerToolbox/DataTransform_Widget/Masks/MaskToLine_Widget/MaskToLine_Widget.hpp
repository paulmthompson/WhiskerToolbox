#ifndef MASKTOLINE_WIDGET_HPP
#define MASKTOLINE_WIDGET_HPP

#include "DataTransform_Widget/TransformParameter_Widget/TransformParameter_Widget.hpp"
#include "DataManager/transforms/Masks/mask_to_line.hpp"
#include "DataManager/DataManager.hpp"

namespace Ui { class MaskToLine_Widget; }

class MaskToLine_Widget : public TransformParameter_Widget {
    Q_OBJECT
public:
    explicit MaskToLine_Widget(QWidget *parent = nullptr);
    ~MaskToLine_Widget() override;

    void setDataManager(std::shared_ptr<DataManager> data_manager);
    [[nodiscard]] std::unique_ptr<TransformParametersBase> getParameters() const override;

private slots:
    void onMethodChanged(int index);

private:
    Ui::MaskToLine_Widget *ui;
    std::shared_ptr<DataManager> _data_manager;
};

#endif // MASKTOLINE_WIDGET_HPP 