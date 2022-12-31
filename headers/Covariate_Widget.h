#ifndef COVARIATE_WIDGET_H
#define COVARIATE_WIDGET_H

#include <QWidget>

#include <iostream>
#include <vector>

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

        config_win = new Covariate_Config(parent);

        connect(pushButton_2,SIGNAL(clicked()),this,SLOT(openConfig()));
    }

private:
    std::vector<float> data;
    float y_max;
    float y_min;
    Covariate_Config* config_win;
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
};
#endif // COVARIATE_WIDGET_H
