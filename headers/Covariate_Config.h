#ifndef COVARIATE_CONFIG_H
#define COVARIATE_CONFIG_H

#include <QWidget>

#include <iostream>
#include <vector>

#include "ui_Covariate_Config.h"

//https://doc.qt.io/qt-6/designer-using-a-ui-file.html
class Covariate_Config : public QWidget, private Ui::Covariate_Config
{
Q_OBJECT
public:
    Covariate_Config(QWidget *parent = 0) : QWidget(parent) {
        setupUi(this);
        //std::cout << graphicsView->size().height() << std::endl;

        // This flag is necessary to make the Widget appear as a separate window
        //https://doc.qt.io/qt-6/qt.html#WindowType-enum
        setWindowFlag(Qt::Window);
    }

private:

private slots:

};


#endif // COVARIATE_CONFIG_H
