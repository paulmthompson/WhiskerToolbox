#ifndef MASK_WIDGET_HPP
#define MASK_WIDGET_HPP

#include <QWidget>

namespace dl {class EfficientSAM;};

#include <filesystem>
#include <memory>
#include <string>

namespace Ui { class Mask_Widget; }

class DataManager;

class Mask_Widget : public QWidget
{
    Q_OBJECT
public:

    Mask_Widget(std::shared_ptr<DataManager> data_manager, QWidget *parent = 0);
    ~Mask_Widget();

    void openWidget(); // Call
    void selectPoint(float const x, float const y);
    void setActiveKey(const std::string &key);
private:
    Ui::Mask_Widget *ui;
    std::shared_ptr<DataManager> _data_manager;
    std::unique_ptr<dl::EfficientSAM> _sam_model;
    std::string _active_key;

private slots:
    void _loadSamModel();
};


#endif // MASK_WIDGET_HPP
