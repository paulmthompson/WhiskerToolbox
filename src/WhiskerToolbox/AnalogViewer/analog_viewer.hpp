//
// Created by wanglab on 4/15/2024.
//

#ifndef WHISKERTOOLBOX_ANALOG_VIEWER_HPP
#define WHISKERTOOLBOX_ANALOG_VIEWER_HPP

#include <QWidget>

#include "Media_Window.h"
#include "TimeFrame.hpp"

#include <memory>

#if defined _WIN32 || defined __CYGWIN__
    #define DLLOPT __declspec(dllexport)
#else
    #define DLLOPT __attribute__((visibility("default")))
#endif

namespace Ui {
class Analog_Viewer;
}

class DLLOPT Analog_Viewer : public QWidget {
    Q_OBJECT
public:
    Analog_Viewer(Media_Window *scene, QWidget *parent = 0);
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
