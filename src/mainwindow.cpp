#include "mainwindow.h"
#include "ui_mainwindow.h"

#include <QFileDialog>
#include <QImage>
#include <QVideoWidget>
#include <QVideoFrameFormat>
#include <QVideoFrame>
#include <QGraphicsPixmapItem>

#include <QTimer>
#include <QElapsedTimer>

#include <stdio.h>
#include <functional>
#include <memory>

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindow)
{
    ui->setupUi(this);

    frame_count = 0;
    play_speed = 1;

    this->selected_whisker = 0;

    wt = std::make_unique<WhiskerTracker>();

    this->scene = new Video_Window(this);

    timer = new QTimer(this);
    connect(timer, &QTimer::timeout, this, &MainWindow::vidLoop);

    play_mode = false;

    ui->graphicsView->setScene(this->scene);
    ui->graphicsView->show();

    selection_mode = Whisker_Select;

    createActions();

}

MainWindow::~MainWindow()
{
    delete ui;

}

void MainWindow::vidLoop()
{
    auto loaded_frame = scene->AdvanceFrame(this->play_speed, true);
    this->selected_whisker = 0;
    ui->frame_label->setText(QString::number(loaded_frame));
}

void MainWindow::createActions()
{
    connect(ui->actionLoad_Video,SIGNAL(triggered()),this,SLOT(Load_Video()));
    connect(ui->horizontalScrollBar,SIGNAL(valueChanged(int)),this,SLOT(Slider_Scroll(int)));
    connect(ui->play_button,SIGNAL(clicked()),this,SLOT(PlayButton()));
    connect(ui->rewind,SIGNAL(clicked()),this,SLOT(RewindButton()));
    connect(ui->fastforward,SIGNAL(clicked()),this,SLOT(FastForwardButton()));
    connect(ui->trace_button,SIGNAL(clicked()),this,SLOT(TraceButton()));
    connect(this->scene,SIGNAL(leftClick(qreal,qreal)),this,SLOT(ClickedInVideo(qreal,qreal)));
}

void MainWindow::Load_Video()
{
    auto vid_name =  QFileDialog::getOpenFileName(
                this,
                "Load Video File",
                QDir::currentPath(),
                "All files (*.*) ;; MP4 (*.mp4)");

    ui->horizontalScrollBar->setMaximum(scene->GetVideoInfo(vid_name.toStdString()));

    scene->LoadFrame(0);
    this->selected_whisker = 0;
}

void MainWindow::PlayButton()
{
    if (play_mode) {

        timer->stop();
        ui->play_button->setText(QString("Play"));
        play_mode = false;

        ui->horizontalScrollBar->blockSignals(true);
        ui->horizontalScrollBar->setValue(scene->getLastLoadedFrame());
        ui->horizontalScrollBar->blockSignals(false);

    } else {
        ui->play_button->setText(QString("Pause"));
        timer->start(40);
        play_mode = true;
    }
}

void MainWindow::RewindButton()
{
    if (this->play_speed > 1)
    {
        this->play_speed--;
        ui->fps_label->setText(QString::number(25 * this->play_speed));
    }
}

void MainWindow::FastForwardButton()
{
    this->play_speed++;
    ui->fps_label->setText(QString::number(25 * this->play_speed));
}

void MainWindow::Slider_Scroll(int newPos)
{
    scene->LoadFrame(newPos);
    this->selected_whisker = 0;
    ui->frame_label->setText(QString::number(newPos));
}

QImage MainWindow::convertToImage(std::vector<uint8_t> input, int width, int height)
{
   return QImage(&input[0],width, height, QImage::Format_Grayscale8);
}

void MainWindow::TraceButton()
{
    QElapsedTimer timer2;
    timer2.start();

   wt->trace(this->scene->getCurrentFrame());

   int t1 = timer2.elapsed();
   DrawWhiskers();

   int t2 = timer2.elapsed();

   qDebug() << "The tracing took" << t1 << "ms and drawing took" << (t2-t1);
}

void MainWindow::DrawWhiskers()
{
    scene->clearLines();

    for (auto& w : wt->whiskers) {

        auto whisker_color = (w.id == this->selected_whisker) ? QPen(QColor(Qt::red)) : QPen(QColor(Qt::blue));

        scene->addLine(w.x,w.y,whisker_color);

    }
}

void MainWindow::ClickedInVideo(qreal x,qreal y) {

    switch(this->selection_mode) {
        case Whisker_Select: {
        std::tuple<float,int> nearest_whisker = wt->get_nearest_whisker(x, y);
        if (std::get<0>(nearest_whisker) < 10.0f) {
            this->selected_whisker = std::get<1>(nearest_whisker);
            this->DrawWhiskers();
        }
        break;
        }
        case Whisker_Pad_Select:

        break;

        default:
        break;
    }
}
