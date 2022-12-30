#ifndef COVARIATE_WIDGET_H
#define COVARIATE_WIDGET_H

#include <QWidget>
#include <QUiLoader>
#include <QFile>
#include "ui_covariate_widget.h"

//https://doc.qt.io/qt-6/designer-using-a-ui-file.html
class Covariate_Widget : public QWidget, private Ui::Covariate_Widget
{
Q_OBJECT
public:
    Covariate_Widget(QWidget *parent = 0) : QWidget(parent) {
        setupUi(this);
    }


};
#endif // COVARIATE_WIDGET_H
