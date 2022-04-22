#include "mainwindow.h"
#include "./ui_mainwindow.h"

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

    QString vid_name;

    frame_count = 0;
    last_loaded_frame = 0;
    play_speed = 1;

    vd = std::make_unique<ffmpeg_wrapper::VideoDecoder>();
    wt = std::make_unique<WhiskerTracker>();
    std::vector<uint8_t>current_frame = {};

    scene = new QGraphicsScene(this);

    timer = new QTimer(this);
    connect(timer, &QTimer::timeout, this, &MainWindow::vidLoop);

    play_mode = false;

    myimage = QImage(640,480,QImage::Format_Grayscale8);
    pixmap_item = scene->addPixmap(QPixmap::fromImage(myimage));
    ui->graphicsView->setScene(scene);
    ui->graphicsView->show();

    createActions();

}

MainWindow::~MainWindow()
{
    delete ui;

}

void MainWindow::vidLoop()
{
    LoadFrame(this->last_loaded_frame + this->play_speed, true);
    ui->frame_label->setText(QString::number(this->last_loaded_frame));
}

void MainWindow::createActions()
{
    connect(ui->actionLoad_Video,SIGNAL(triggered()),this,SLOT(Load_Video()));
    connect(ui->horizontalScrollBar,SIGNAL(valueChanged(int)),this,SLOT(Slider_Scroll(int)));
    connect(ui->play_button,SIGNAL(clicked()),this,SLOT(PlayButton()));
    connect(ui->rewind,SIGNAL(clicked()),this,SLOT(RewindButton()));
    connect(ui->fastforward,SIGNAL(clicked()),this,SLOT(FastForwardButton()));
    connect(ui->trace_button,SIGNAL(clicked()),this,SLOT(TraceButton()));
}

void MainWindow::Load_Video()
{
    vid_name =  QFileDialog::getOpenFileName(
                this,
                "Load Video File",
                QDir::currentPath(),
                "All files (*.*) ;; MP4 (*.mp4)");

    GetVideoInfo();
    current_frame.resize(vd->getHeight()*vd->getWidth());

    LoadFrame(0);
}

void MainWindow::PlayButton()
{
    if (play_mode) {

        timer->stop();
        ui->play_button->setText(QString("Play"));
        play_mode = false;

        ui->horizontalScrollBar->blockSignals(true);
        ui->horizontalScrollBar->setValue(last_loaded_frame);
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
void MainWindow::UpdateCanvas(QImage& img)
{
    for (auto pathItem : this->whisker_paths) {
        scene->removeItem(pathItem);
    }
    this->whisker_paths.clear();
    pixmap_item->setPixmap(QPixmap::fromImage(img));
}

void MainWindow::Slider_Scroll(int newPos)
{
    LoadFrame(newPos);
    ui->frame_label->setText(QString::number(newPos));
}

void MainWindow::GetVideoInfo()
{
    this->vd->createMedia(this->vid_name.toStdString());

    ui->horizontalScrollBar->setMaximum(vd->getFrameCount());
}

void MainWindow::LoadFrame(int frame_id,bool frame_by_frame)
{
    std::vector<uint8_t> image = vd->getFrame( frame_id, frame_by_frame);

    this->current_frame = image;

    QImage img = QImage(&image[0],vd->getWidth(), vd->getHeight(), QImage::Format_Grayscale8);
    UpdateCanvas(img);
    this->last_loaded_frame = frame_id;
}

QImage MainWindow::convertToImage(std::vector<uint8_t> input, int width, int height)
{
   return QImage(&input[0],width, height, QImage::Format_Grayscale8);
}

void MainWindow::TraceButton()
{
    QElapsedTimer timer2;
    timer2.start();

   wt->trace(this->current_frame);

   int t1 = timer2.elapsed();
   DrawWhiskers();

   int t2 = timer2.elapsed();

   qDebug() << "The tracing took" << t1 << "ms and drawing took" << (t2-t1);
}

void MainWindow::DrawWhiskers()
{
    for (auto pathItem : this->whisker_paths) {
        scene->removeItem(pathItem);
    }
    this->whisker_paths.clear();

    for (auto& w : wt->whiskers) {
        QPainterPath* path = new QPainterPath();

        path->moveTo(QPointF(w.x[0],w.y[0]));

        for (int i = 1; i < w.x.size(); i++) {
            path->lineTo(QPointF(w.x[i],w.y[i]));
        }

        whisker_paths.append(this->scene->addPath(*path,QPen(QColor(Qt::blue))));
    }
}
