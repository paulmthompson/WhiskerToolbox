#ifndef LINEALIGNMENT_WIDGET_HPP
#define LINEALIGNMENT_WIDGET_HPP

#include "DataTransform_Widget/TransformParameter_Widget/DataManagerParameter_Widget.hpp"

#include <memory>
#include <string>

class DataManager;

namespace Ui {
class LineAlignment_Widget;
}


class LineAlignment_Widget : public DataManagerParameter_Widget {
    Q_OBJECT
public:
    explicit LineAlignment_Widget(QWidget * parent = nullptr);
    ~LineAlignment_Widget() override;

    [[nodiscard]] std::unique_ptr<TransformParametersBase> getParameters() const override;

protected:
    void onDataManagerDataChanged() override;

private:
    Ui::LineAlignment_Widget * ui;
    std::string _selected_media_key;
    DataManager * _data_manager = nullptr;

    void _refreshMediaDataKeys();
    void _updateMediaDataKeyComboBox();

private slots:
    void _widthValueChanged(int value);
    void _perpendicularRangeValueChanged(int value);
    void _useProcessedDataToggled(bool checked);
    void _approachChanged(int index);
    void _outputModeChanged(int index);
    void _mediaDataKeyChanged(int index);
};

#endif// LINEALIGNMENT_WIDGET_HPP
