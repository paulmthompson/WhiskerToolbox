#ifndef COVARIATE_CONFIG_H
#define COVARIATE_CONFIG_H


#include <QWidget>

#include <iostream>
#include <vector>
#include <memory>

#include "ui_Covariate_Config.h"

struct config_options {
    float y_max;
    float y_min;
    config_options() {
        y_max = 10.0;
        y_min = -10.0;
    };
};

//https://doc.qt.io/qt-6/designer-using-a-ui-file.html
class Covariate_Config : public QWidget, private Ui::Covariate_Config
{
Q_OBJECT
public:
    Covariate_Config(std::shared_ptr<config_options> opts, QWidget *parent = 0) : QWidget(parent) {
        setupUi(this);
        //std::cout << graphicsView->size().height() << std::endl;
        this->c_opt = opts;
        // This flag is necessary to make the Widget appear as a separate window
        //https://doc.qt.io/qt-6/qt.html#WindowType-enum
        setWindowFlag(Qt::Window);

        connect(
            y_max_spin,QOverload<double>::of(&QDoubleSpinBox::valueChanged),
                    this,[=]( const double d ) { this->c_opt->y_max = d; emit valueChanged();}
        );

        connect(
            y_min_spin,QOverload<double>::of(&QDoubleSpinBox::valueChanged),
                    this,[=]( const double d ) { this->c_opt->y_min = d; emit valueChanged();}
        );

    };
    void updateValues() {
        this->y_max_spin->setValue(this->c_opt->y_max);
        this->y_min_spin->setValue(this->c_opt->y_min);
    };

private:
    std::shared_ptr<config_options> c_opt;

private slots:

signals:
    void valueChanged();

};


#endif // COVARIATE_CONFIG_H
