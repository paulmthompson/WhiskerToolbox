//
// Created by wanglab on 4/15/2024.
//

#ifndef WHISKERTOOLBOX_ANALOG_VIEWER_HPP
#define WHISKERTOOLBOX_ANALOG_VIEWER_HPP

#include <QWidget>

#include "Media_Window.h"
#include "TimeFrame.h"

#include <memory>

namespace Ui {
class Analog_Viewer;
}

class Analog_Viewer : public QWidget {
    Q_OBJECT
public:
    Analog_Viewer(Media_Window *scene, std::shared_ptr<TimeFrame> time, QWidget *parent = 0);
    ~Analog_Viewer();

    void openWidget();

protected:
    //void closeEvent(QCloseEvent *event);
    //void keyPressEvent(QKeyEvent *event);

private:
    Media_Window *_scene;
    std::shared_ptr<TimeFrame> _time;

    Ui::Analog_Viewer *ui;
private slots:

};



#endif //WHISKERTOOLBOX_ANALOG_VIEWER_HPP
