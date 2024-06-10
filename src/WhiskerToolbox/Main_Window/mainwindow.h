#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QImage>
#include <QGraphicsScene>
#include <QTimer>
#include <QPointer>

#include "Media_Window.h"
#include "Whisker_Widget.h"
#include "Label_Widget.hpp"
#include "analog_viewer.hpp"
#include "Image_Processing_Widget/Image_Processing_Widget.hpp"

#include "TimeFrame.hpp"
#include "DataManager.hpp"

QT_BEGIN_NAMESPACE
namespace Ui { class MainWindow; }
QT_END_NAMESPACE

class MainWindow : public QMainWindow
{
    Q_OBJECT
public:
    MainWindow(QWidget *parent = nullptr);
    ~MainWindow();

protected:
    void keyPressEvent(QKeyEvent *event);

private:
    Ui::MainWindow *ui;

    Media_Window* _scene;
    QPointer<Whisker_Widget> _ww;
    QPointer<Label_Widget> _label_maker;
    QPointer<Analog_Viewer> _analog_viewer;
    QPointer<Image_Processing_Widget> _image_processing;

    void _updateMedia();

    void _createActions();

    void _LoadData(std::string filepath);

    bool _verbose;

    std::shared_ptr<DataManager> _data_manager;


private slots:
    void Load_Video();
    void Load_Images();

    void openWhiskerTracking();
    void openLabelMaker();
    void openAnalogViewer();
    void openImageProcessing();
};
#endif // MAINWINDOW_H
