#ifndef HDF5MASKSAVER_WIDGET_HPP
#define HDF5MASKSAVER_WIDGET_HPP

#include <QWidget>

namespace Ui {
class HDF5MaskSaver_Widget;
}

class HDF5MaskSaver_Widget : public QWidget {
    Q_OBJECT

public:
    explicit HDF5MaskSaver_Widget(QWidget *parent = nullptr);
    ~HDF5MaskSaver_Widget() override;

private:
    Ui::HDF5MaskSaver_Widget *ui;
};

#endif // HDF5MASKSAVER_WIDGET_HPP 