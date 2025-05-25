#ifndef HDF5_MASK_LOADER_WIDGET_HPP
#define HDF5_MASK_LOADER_WIDGET_HPP

#include <QWidget>
#include <QString>

namespace Ui {
class HDF5MaskLoader_Widget;
}

class HDF5MaskLoader_Widget : public QWidget {
    Q_OBJECT
public:
    explicit HDF5MaskLoader_Widget(QWidget *parent = nullptr);
    ~HDF5MaskLoader_Widget() override;

signals:
    void loadSingleHDF5MaskRequested();
    void loadMultiHDF5MaskRequested(QString pattern);

private:
    Ui::HDF5MaskLoader_Widget *ui;
};

#endif // HDF5_MASK_LOADER_WIDGET_HPP 
