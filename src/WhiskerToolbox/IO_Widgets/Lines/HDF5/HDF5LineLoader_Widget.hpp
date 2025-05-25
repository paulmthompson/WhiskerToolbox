#ifndef HDF5LINELOADER_WIDGET_HPP
#define HDF5LINELOADER_WIDGET_HPP

#include <QString>
#include <QWidget>


namespace Ui {
class HDF5LineLoader_Widget;
}


class HDF5LineLoader_Widget : public QWidget {
    Q_OBJECT
public:
    explicit HDF5LineLoader_Widget(QWidget * parent = nullptr);
    ~HDF5LineLoader_Widget() override;


signals:
    void newHDF5Filename(QString filename);
    void newHDF5MultiFilename(QString dir_name, QString pattern);

private:
    Ui::HDF5LineLoader_Widget * ui;

private slots:
    void _loadSingleHdf5Line();
    void _loadMultiHdf5Line();
};

#endif// HDF5LINELOADER_WIDGET_HPP
