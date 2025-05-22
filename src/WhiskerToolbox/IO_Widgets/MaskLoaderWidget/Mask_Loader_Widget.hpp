#ifndef MASK_LOADER_WIDGET_HPP
#define MASK_LOADER_WIDGET_HPP


#include <QWidget>

#include <memory>
#include <string>


class DataManager;

namespace Ui {
class Mask_Loader_Widget;
}

class Mask_Loader_Widget : public QWidget {
    Q_OBJECT
public:
    explicit Mask_Loader_Widget(std::shared_ptr<DataManager> data_manager, QWidget * parent = nullptr);
    ~Mask_Loader_Widget() override;

private:
    Ui::Mask_Loader_Widget * ui;
    std::shared_ptr<DataManager> _data_manager;

    void _loadSingleHDF5Mask(std::string const & filename, std::string const & mask_suffix = "");

private slots:
    void _loadSingleHdf5Mask();
    void _loadMultiHdf5Mask();
    void _enableImageScaling(bool enable);
};

#endif// MASK_LOADER_WIDGET_HPP
