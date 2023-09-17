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

    last_loaded_frame = 0; // This should be in data / time object
    total_frame_count = 0; // This should be in data / time object

    verbose_frame = false;
}

void Media_Window::addLine(QPainterPath* path, QPen color) {
    auto linePath = addPath(*path,color);
    line_paths.append(linePath);
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

int Media_Window::LoadImages(std::string name) {

    this->media = std::make_shared<ImageSeries>();

    this->total_frame_count = this->media->LoadMedia(name);
    return this->total_frame_count;
}

//Jump to specific frame designated by frame_id
int Media_Window::LoadFrame(int frame_id , bool relative)
{

    if (relative) {
        frame_id = this->last_loaded_frame + frame_id;
    }

    if (frame_id < 0) {
        frame_id = 0;
    } else if (frame_id >= this->total_frame_count - 1) {
        frame_id = this->total_frame_count -1;
    }

    frame_id = this->media->LoadFrame(frame_id);
    //frame_id = doLoadFrame(frame_id);

    //Get current_frame (in media coordinates)
    auto image_native_resolution = QImage(&(this->media->getCurrentFrame())[0],
                                          this->media->getWidth(),
                                          this->media->getHeight(), QImage::Format_Grayscale8);
    this->myimage = image_native_resolution.scaled(this->canvasWidth,this->canvasHeight);
    //auto image_native_resolution = QImage(&this->current_frame[0],vd->getWidth(), vd->getHeight(), QImage::Format_Grayscale8);
    //this->myimage = image_native_resolution.scaled(this->canvasWidth,this->canvasHeight);

    //Scale it to canvas image
    //update canvas


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

float Media_Window::getXAspect() const {

    float scale_width = static_cast<float>(this->canvasWidth)
                        / static_cast<float>(std::get<1>(this->getMediaDimensions()));

    return scale_width;
}

float Media_Window::getYAspect() const {

    float scale_height = static_cast<float>(this->canvasHeight)
                         / static_cast<float>(std::get<0>(this->getMediaDimensions()));

    return scale_height;
}

