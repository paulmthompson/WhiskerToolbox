#ifndef LABEL_WIDGET_H
#define LABEL_WIDGET_H

#include <QWidget>

#include <memory>

#include "Video_Window.h"

#include "ui_Label_Widget.h"

class Label_Widget : public QWidget, private Ui::Label_Widget
{
    Q_OBJECT
public:
   Label_Widget(Video_Window* scene, QWidget *parent = 0) : QWidget(parent) {
        setupUi(this);

        this->scene = scene;

    };

   void openWidget(); // Call

protected:
   void closeEvent(QCloseEvent *event);

private:
    Video_Window * scene;

private slots:
    void ClickedInVideo(qreal x,qreal y);

};

#endif // LABEL_WIDGET_H
