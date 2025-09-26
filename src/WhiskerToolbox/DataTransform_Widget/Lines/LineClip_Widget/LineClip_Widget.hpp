#ifndef LINECLIP_WIDGET_HPP
#define LINECLIP_WIDGET_HPP

#include "DataTransform_Widget/TransformParameter_Widget/DataManagerParameter_Widget.hpp"

class DataManager;
namespace Ui { class LineClip_Widget; }

class LineClip_Widget : public DataManagerParameter_Widget {
    Q_OBJECT
public:
    explicit LineClip_Widget(QWidget *parent = nullptr);
    ~LineClip_Widget() override;

    [[nodiscard]] std::unique_ptr<TransformParametersBase> getParameters() const override;

protected:
    void onDataManagerChanged() override;
    void onDataManagerDataChanged() override;

private slots:
    void _lineFeatureSelected(QString const & feature);
    void onClipSideChanged(int index);

private:
    Ui::LineClip_Widget *ui;
};

#endif // LINECLIP_WIDGET_HPP
