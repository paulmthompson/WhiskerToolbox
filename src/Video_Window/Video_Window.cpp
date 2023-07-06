#include <QGraphicsScene>
#include <QGraphicsSceneMouseEvent>
#include <QGraphicsPixmapItem>
#include <QImage>

#include "Video_Window.h"

#include <ffmpeg_wrapper/videodecoder.h>

/*

The Video_Window class

*/

Video_Window::Video_Window(QObject *parent) : QGraphicsScene(parent) {

    w = 640;
    h = 480;
    this->myimage = QImage(w,h,QImage::Format_Grayscale8);
    this->pixmap_item = addPixmap(QPixmap::fromImage(this->myimage));

    vd = std::make_unique<ffmpeg_wrapper::VideoDecoder>();
    last_loaded_frame = 0;
}

void Video_Window::addLine(QPainterPath* path, QPen color) {
    line_paths.append(addPath(*path,color));
}

void Video_Window::clearLines() {
    for (auto pathItem : this->line_paths) {
        removeItem(pathItem);
    }
    this->line_paths.clear();
}

template <typename T>
void Video_Window::addPoint(T x, T y, QPen color) {
    this->points.append(addEllipse(x,y,15.0,15.0,color));
}

void Video_Window::clearPoints() {
    for (auto pathItem : this->points) {
        removeItem(pathItem);
    }
    this->points.clear();
}

void Video_Window::UpdateCanvas()
{
    clearLines();
    clearPoints();
    QImage img = QImage(&this->current_frame[0],vd->getWidth(), vd->getHeight(), QImage::Format_Grayscale8);
    UpdateCanvas(img);
    this->pixmap_item->setPixmap(QPixmap::fromImage(img));
}

void Video_Window::UpdateCanvas(QImage& img)
{
    clearLines();
    clearPoints();
    this->pixmap_item->setPixmap(QPixmap::fromImage(img));
}

std::vector<uint8_t> Video_Window::getCurrentFrame() const {
    return this->current_frame;
}

int Video_Window::GetVideoInfo(std::string name)
{
    this->vid_name = name;
    this->vd->createMedia(this->vid_name);

    this->current_frame.resize(vd->getHeight()*vd->getWidth());

    return vd->getFrameCount();
}

// Advance from current frame by num_frames
int Video_Window::AdvanceFrame(int num_frames) {
    return LoadFrame(this->last_loaded_frame + num_frames, true);
}

//Jump to specific frame designated by frame_id
int Video_Window::LoadFrame(int frame_id,bool frame_by_frame)
{

    std::cout << "Getting frame " << frame_id << std::endl;

    this->current_frame = vd->getFrame( frame_id, frame_by_frame);

    std::cout << "Loaded frame " << frame_id << std::endl;

    QImage img = QImage(&this->current_frame[0],vd->getWidth(), vd->getHeight(), QImage::Format_Grayscale8);
    UpdateCanvas(img);

    std::cout << "Drew frame " << frame_id << std::endl;

    this->last_loaded_frame = frame_id;
    return this->last_loaded_frame;
}

int Video_Window::getLastLoadedFrame() const {
    return last_loaded_frame;
}

int Video_Window::findNearestKeyframe(int frame) const {
    return this->vd->nearest_iframe(frame);
}

void Video_Window::mousePressEvent(QGraphicsSceneMouseEvent *event) {
    if (event->button() == Qt::LeftButton) {
        emit leftClick(event->scenePos().x(),event->scenePos().y());
    } else if (event->button() == Qt::RightButton){

    }
}
void Video_Window::mouseReleaseEvent(QGraphicsSceneMouseEvent *event) {

}
void Video_Window::mouseMoveEvent(QGraphicsSceneMouseEvent *event) {

}


