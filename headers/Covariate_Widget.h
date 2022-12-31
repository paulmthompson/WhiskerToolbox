#ifndef COVARIATE_WIDGET_H
#define COVARIATE_WIDGET_H

#include <QWidget>

#include <iostream>
#include <vector>
#include <memory>

#include "Covariate_Config.h"

#include "ui_covariate_widget.h"

//https://doc.qt.io/qt-6/designer-using-a-ui-file.html
class Covariate_Widget : public QWidget, private Ui::Covariate_Widget
{
Q_OBJECT
public:
    Covariate_Widget(QWidget *parent = 0) : QWidget(parent) {
        setupUi(this);
        //std::cout << graphicsView->size().height() << std::endl;

        this->c_opt = std::make_shared<config_options>();

        config_win = new Covariate_Config(c_opt,this);

        connect(pushButton_2,SIGNAL(clicked()),this,SLOT(openConfig()));

        connect(config_win,SIGNAL(valueChanged()),this,SLOT(updateValues()));
    }

private:
    std::vector<float> data;
    Covariate_Config* config_win;
    std::shared_ptr<config_options> c_opt;
    //Gain?
    //Offset?
    //Time
    //Plot type (analog, digital)
    //Color
    //
private slots:
    void openConfig() {
        config_win->show();
    };
    void updateValues() {
        std::cout << "Y Max: " << c_opt->y_max << std::endl;
        std::cout << "Y Min: " << c_opt->y_min << std::endl;
    };
};
#endif // COVARIATE_WIDGET_H
