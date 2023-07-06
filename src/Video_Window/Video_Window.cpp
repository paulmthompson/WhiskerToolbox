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

    canvasWidth = 640;
    canvasHeight = 480;
    this->myimage = QImage(canvasWidth,canvasHeight,QImage::Format_Grayscale8);
    this->pixmap_item = addPixmap(QPixmap::fromImage(this->myimage));

    vd = std::make_unique<ffmpeg_wrapper::VideoDecoder>();
    last_loaded_frame = 0;
    frame_number = 0;
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

void Video_Window::clearPoints() {
    for (auto pathItem : this->points) {
        removeItem(pathItem);
    }
    this->points.clear();
}

void Video_Window::UpdateCanvas()
{
    QImage img = QImage(&this->current_frame[0],vd->getWidth(), vd->getHeight(), QImage::Format_Grayscale8);
    UpdateCanvas(img);
}

void Video_Window::UpdateCanvas(QImage& img)
{
    clearLines();
    clearPoints();
    //We should resize image here to match the size of the canvas
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


    this->frame_number =  vd->getFrameCount(); // Total frames
    return this->frame_number;
}

// Advance from current frame by num_frames or reverse
// For forward, we should just keep decoding, but in reverse
// We will always need to seek to a new keyframe
int Video_Window::AdvanceFrame(int num_frames) {
    if (num_frames > 0) {
        return LoadFrame(this->last_loaded_frame + num_frames, true);
    } else {
        return LoadFrame(this->last_loaded_frame + num_frames, false);
    }
}

//Jump to specific frame designated by frame_id
int Video_Window::LoadFrame(int frame_id,bool frame_by_frame)
{

    if (frame_id < 0) {
        frame_id = 0;
        frame_by_frame = false;
    } else if (frame_id >= this->frame_number - 1) {
        frame_id = this->frame_number -1;
        frame_by_frame = false;
    }

    std::cout << "Getting frame " << frame_id << std::endl;

    this->current_frame = vd->getFrame( frame_id, frame_by_frame);

    std::cout << "Loaded frame " << frame_id << std::endl;

    QImage img = QImage(&this->current_frame[0],vd->getWidth(), vd->getHeight(), QImage::Format_Grayscale8);
    UpdateCanvas(img);

    std::cout << "Drew frame " << frame_id << std::endl;

    this->last_loaded_frame = frame_id;
    return this->last_loaded_frame;
    // I should emit a signal here that can be caught by anything that draws to scene (before or after draw? or both?)
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

    } else {
        QGraphicsScene::mousePressEvent(event);
    }
}
void Video_Window::mouseReleaseEvent(QGraphicsSceneMouseEvent *event) {
    QGraphicsScene::mouseReleaseEvent(event);
}
void Video_Window::mouseMoveEvent(QGraphicsSceneMouseEvent *event) {
    QGraphicsScene::mouseMoveEvent(event);

}


