#ifndef LINECLIP_WIDGET_HPP
#define LINECLIP_WIDGET_HPP

#include "DataTransform_Widget/TransformParameter_Widget/TransformParameter_Widget.hpp"
#include "DataManager/transforms/Lines/line_clip.hpp"
#include "DataManager/DataManager.hpp"

namespace Ui { class LineClip_Widget; }

class LineClip_Widget : public TransformParameter_Widget {
    Q_OBJECT
public:
    explicit LineClip_Widget(QWidget *parent = nullptr);
    ~LineClip_Widget() override;

    void setDataManager(std::shared_ptr<DataManager> data_manager);
    [[nodiscard]] std::unique_ptr<TransformParametersBase> getParameters() const override;

private slots:
    void _lineFeatureSelected(QString const & feature);
    void onClipSideChanged(int index);

private:
    Ui::LineClip_Widget *ui;
    std::shared_ptr<DataManager> _data_manager;
};

#endif // LINECLIP_WIDGET_HPP 