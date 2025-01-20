#ifndef MASK_LOADER_WIDGET_HPP
#define MASK_LOADER_WIDGET_HPP


#include <QWidget>

#include <memory>
#include <string>


class DataManager;

namespace Ui {
class Mask_Loader_Widget;
}

class Mask_Loader_Widget : public QWidget
{
    Q_OBJECT
public:

    Mask_Loader_Widget(std::shared_ptr<DataManager> data_manager, QWidget *parent = 0);
    ~Mask_Loader_Widget();

private:
    Ui::Mask_Loader_Widget *ui;
    std::shared_ptr<DataManager> _data_manager;

    void _loadSingleHDF5Mask(std::string filename);

private slots:
    void _loadSingleHdf5Mask();
};

#endif // MASK_LOADER_WIDGET_HPP
