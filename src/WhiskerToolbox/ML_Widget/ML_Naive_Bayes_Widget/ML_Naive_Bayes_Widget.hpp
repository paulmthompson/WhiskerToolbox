#ifndef ML_NAIVE_BAYES_WIDGET_HPP
#define ML_NAIVE_BAYES_WIDGET_HPP

#include <QWidget>

#include <memory>

class DataManager;

namespace Ui { class ML_Naive_Bayes_Widget; }

class ML_Naive_Bayes_Widget : public QWidget {
    Q_OBJECT

public:
    ML_Naive_Bayes_Widget(std::shared_ptr<DataManager> data_manager,
                            QWidget *parent = 0);

    virtual ~ML_Naive_Bayes_Widget();

protected:

private slots:

private:
    std::shared_ptr<DataManager> _data_manager;
    Ui::ML_Naive_Bayes_Widget *ui;

};



#endif // ML_NAIVE_BAYES_WIDGET_HPP
