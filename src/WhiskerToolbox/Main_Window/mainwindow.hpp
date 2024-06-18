#ifndef MAINWINDOW_HPP
#define MAINWINDOW_HPP

#include <QMainWindow>
#include <QImage>
#include <QGraphicsScene>
#include <QTimer>
#include <QPointer>

#include "Media_Window.hpp"
#include "Whisker_Widget.hpp"
#include "Label_Widget.hpp"
#include "analog_viewer.hpp"
#include "Image_Processing_Widget/Image_Processing_Widget.hpp"
#include "Tongue_Widget/Tongue_Widget.hpp"

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
    QPointer<Tongue_Widget> _tongue_widget;

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
    void openTongueTracking();
};
#endif // MAINWINDOW_HPP
