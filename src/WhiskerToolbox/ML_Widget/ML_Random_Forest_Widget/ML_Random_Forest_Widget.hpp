#ifndef ML_RANDOM_FOREST_WIDGET_HPP
#define ML_RANDOM_FOREST_WIDGET_HPP

#include <QWidget>

#include <memory>

class DataManager;

namespace Ui { class ML_Random_Forest_Widget; }

class ML_Random_Forest_Widget : public QWidget {
    Q_OBJECT

public:
    ML_Random_Forest_Widget(std::shared_ptr<DataManager> data_manager,
              QWidget *parent = 0);

    virtual ~ML_Random_Forest_Widget();

protected:

private slots:

private:
    std::shared_ptr<DataManager> _data_manager;
    Ui::ML_Random_Forest_Widget *ui;

};


#endif // ML_RANDOM_FOREST_WIDGET_HPP
