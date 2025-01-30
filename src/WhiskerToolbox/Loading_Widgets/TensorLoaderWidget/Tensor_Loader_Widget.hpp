#ifndef TENSOR_LOADER_WIDGET_HPP
#define TENSOR_LOADER_WIDGET_HPP

#include <QWidget>
#include <memory>

class DataManager;

namespace Ui {
class Tensor_Loader_Widget;
}

class Tensor_Loader_Widget : public QWidget
{
    Q_OBJECT
public:
    Tensor_Loader_Widget(std::shared_ptr<DataManager> data_manager, QWidget *parent = nullptr);
    ~Tensor_Loader_Widget();

private:
    Ui::Tensor_Loader_Widget *ui;
    std::shared_ptr<DataManager> _data_manager;

private slots:
    void _loadNumpyArray();
};

#endif // TENSOR_LOADER_WIDGET_HPP
