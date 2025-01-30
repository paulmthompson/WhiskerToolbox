#ifndef TENSOR_WIDGET_HPP
#define TENSOR_WIDGET_HPP

#include <QWidget>
#include <memory>
#include <string>

namespace Ui { class Tensor_Widget; }

class DataManager;
class TensorTableModel;

class Tensor_Widget : public QWidget
{
    Q_OBJECT
public:
    explicit Tensor_Widget(std::shared_ptr<DataManager> data_manager, QWidget *parent = nullptr);
    ~Tensor_Widget();

    void openWidget(); // Call to open the widget

    void setActiveKey(const std::string &key);
    void updateTable();

private:
    Ui::Tensor_Widget *ui;
    std::shared_ptr<DataManager> _data_manager;
    TensorTableModel* _tensor_table_model;
    std::string _active_key;

private slots:
    void _saveTensorCSV();
};

#endif // TENSOR_WIDGET_HPP
