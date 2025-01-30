#ifndef MASK_WIDGET_HPP
#define MASK_WIDGET_HPP

#include <QWidget>


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
private:
    Ui::Mask_Widget *ui;
    std::shared_ptr<DataManager> _data_manager;

private slots:

};


#endif // MASK_WIDGET_HPP
