#ifndef WHISKERTOOLBOX_DATATRANSFORM_WIDGET_HPP
#define WHISKERTOOLBOX_DATATRANSFORM_WIDGET_HPP

#include <QWidget>

#include <memory>
#include <string>

namespace Ui {
class DataTransform_Widget;
}

class DataManager;


class DataTransform_Widget : public QWidget {
    Q_OBJECT
public:
    DataTransform_Widget(std::shared_ptr<DataManager> data_manager,
                         QWidget * parent = nullptr);
    ~DataTransform_Widget() override;

    void openWidget();// Call

private:
    Ui::DataTransform_Widget * ui;
    std::shared_ptr<DataManager> _data_manager;


private slots:
};


#endif//WHISKERTOOLBOX_DATATRANSFORM_WIDGET_HPP
