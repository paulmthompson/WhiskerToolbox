#include <QGraphicsScene>
#include <QGraphicsSceneMouseEvent>
#include <QGraphicsPixmapItem>
#include <QImage>

#include "Media_Window.h"

#include <iostream>

/*

The Media_Window class

*/

Media_Window::Media_Window(QObject *parent) : QGraphicsScene(parent) {

    canvasWidth = 640;
    canvasHeight = 480;
    this->myimage = QImage(canvasWidth,canvasHeight,QImage::Format_Grayscale8);
    this->pixmap_item = addPixmap(QPixmap::fromImage(this->myimage));

    last_loaded_frame = 0;
    total_frame_count = 0;

    verbose_frame = false;
}

void Media_Window::addLine(QPainterPath* path, QPen color) {
    line_paths.append(addPath(*path,color));
}

void Media_Window::clearLines() {
    for (auto pathItem : this->line_paths) {
        removeItem(pathItem);
    }
    this->line_paths.clear();
}

void Media_Window::clearPoints() {
    for (auto pathItem : this->points) {
        removeItem(pathItem);
    }
    this->points.clear();
}

void Media_Window::UpdateCanvas(QImage& img)
{
    clearLines();
    clearPoints();
    //We should resize image here to match the size of the canvas
    this->pixmap_item->setPixmap(QPixmap::fromImage(img));
}

std::vector<uint8_t> Media_Window::getCurrentFrame() const {
    return this->current_frame;
}

int Media_Window::LoadMedia(std::string name) {

    this->total_frame_count = this->doLoadMedia(name);
    return this->total_frame_count;
}

// Advance from current frame by num_frames or reverse
// For forward, we should just keep decoding, but in reverse
// We will always need to seek to a new keyframe
int Media_Window::AdvanceFrame(int num_frames) {
        return LoadFrame(this->last_loaded_frame + num_frames);
}

//Jump to specific frame designated by frame_id
int Media_Window::LoadFrame(int frame_id)
{

    if (frame_id < 0) {
        frame_id = 0;
    } else if (frame_id >= this->total_frame_count - 1) {
        frame_id = this->total_frame_count -1;
    }

    frame_id = doLoadFrame(frame_id);

    UpdateCanvas(this->myimage);

    if (this->verbose_frame) {
        std::cout << "Drew frame " << frame_id << std::endl;
    }

    this->last_loaded_frame = frame_id;
    return this->last_loaded_frame;
    // I should emit a signal here that can be caught by anything that draws to scene (before or after draw? or both?)
}

int Media_Window::getLastLoadedFrame() const {
    return last_loaded_frame;
}

int Media_Window::findNearestSnapFrame(int frame) const {
    return doFindNearestSnapFrame(frame);
}

std::string Media_Window::getFrameID(int frame) {
    return doGetFrameID(frame);
}

void Media_Window::mousePressEvent(QGraphicsSceneMouseEvent *event) {
    if (event->button() == Qt::LeftButton) {
        emit leftClick(event->scenePos().x(),event->scenePos().y());
    } else if (event->button() == Qt::RightButton){

    } else {
        QGraphicsScene::mousePressEvent(event);
    }
}
void Media_Window::mouseReleaseEvent(QGraphicsSceneMouseEvent *event) {
    QGraphicsScene::mouseReleaseEvent(event);
}
void Media_Window::mouseMoveEvent(QGraphicsSceneMouseEvent *event) {
    QGraphicsScene::mouseMoveEvent(event);

}

std::pair<int,int> Media_Window::getMediaDimensions() const {
    return this-> doGetMediaDimensions();
}


