//
// Created by wanglab on 4/15/2024.
//

#ifndef WHISKERTOOLBOX_ANALOG_VIEWER_HPP
#define WHISKERTOOLBOX_ANALOG_VIEWER_HPP

#include <QWidget>


#if defined _WIN32 || defined __CYGWIN__
    #define ANALOG_VIEWER_DLLOPT
    //#define ANALOG_VIEWER_DLLOPT Q_DECL_EXPORT
#else
    #define ANALOG_VIEWER_DLLOPT __attribute__((visibility("default")))
#endif

namespace Ui {
class Analog_Viewer;
}

class ANALOG_VIEWER_DLLOPT Analog_Viewer : public QWidget {
    Q_OBJECT
public:
    Analog_Viewer(QWidget *parent = 0);
    ~Analog_Viewer();

    void openWidget();

protected:
    //void closeEvent(QCloseEvent *event);
    //void keyPressEvent(QKeyEvent *event);

private:

    Ui::Analog_Viewer *ui;
private slots:

};



#endif //WHISKERTOOLBOX_ANALOG_VIEWER_HPP
