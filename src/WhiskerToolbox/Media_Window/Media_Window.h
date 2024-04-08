#ifndef MEDIA_WINDOW_H
#define MEDIA_WINDOW_H

#include <QGraphicsScene>
#include <QGraphicsSceneMouseEvent>
#include <QGraphicsPixmapItem>
#include <QImage>

#include <string>
#include <vector>
#include <memory>
#include <utility>
#include <iostream>


class MediaData {
public:

    std::string getFilename() const {return _filename;};
    void setFilename(std::string filename) {_filename = filename;};

    void setFormat(int format) {_format = format;};
    int getFormat() const {return _format;};

    int getHeight() const {return _height;};
    int getWidth() const {return _width;};
    void updateHeight(int height) {_height = height;};
    void updateWidth(int width) {_width = width;};

    int getTotalFrameCount() const {return _totalFrameCount;};
    void setTotalFrameCount(int total_frame_count) {_totalFrameCount = total_frame_count;};

    std::vector<uint8_t> getData() const {return this->data;};

    virtual int LoadMedia(std::string name) {return 0;};
    virtual void LoadFrame(int frame_id) {};
    virtual std::string GetFrameID(int frame_id) {return "";};

protected:


    std::vector<uint8_t> data;
private:
    std::string _filename;
    int _totalFrameCount;

    int _height;
    int _width;
    int _format; // This corresponds to an enum. Here we will use QImage.
};

/*

The Media_Window class is responsible for plotting images, movies, and shapes on top of them.
Shapes may take the form of lines, points, or arbitrary 2d masks.

Advancing a frame will result in the video window loading new data.

*/

class Media_Window : public QGraphicsScene
{
Q_OBJECT
public:
    Media_Window(QObject *parent = 0);

    //Template member functions must be defined in header file so that compiler than create specialized versions
    //Other methods can achieve this
    template <typename T>
    void addLine(std::vector<T>& x, std::vector<T>& y, QPen color) {
        QPainterPath* path = new QPainterPath();

        auto xAspect = getXAspect();
        auto yAspect = getYAspect();

        path->moveTo(QPointF(static_cast<float>(x[0]) * xAspect, static_cast<float>(y[0]) * yAspect));

        for (int i = 1; i < x.size(); i++) {
            path->lineTo(QPointF(static_cast<float>(x[i]) * xAspect , static_cast<float>(y[i]) * yAspect));
        }

        addLine(path,color);
    }

    void addLine(QPainterPath* path, QPen color);

    void clearLines();

    template <typename T>
    void addPoint(T x_canvas, T y_canvas, QPen color,float radius = 15.0) {

        auto xAspect = getXAspect();
        auto yAspect = getYAspect();

        // addEllipse draws from top left of rectangle down and to the right, so we want to center point in the middle of this rectangle
        _points.append(addEllipse(static_cast<float>(x_canvas) * xAspect - radius/2,
                                       static_cast<float>(y_canvas) * yAspect - radius/2,
                                       radius, radius,color));
    }

    void clearPoints();

    void UpdateCanvas(QImage& img);

    int LoadMedia(std::string name);

    //Jump to specific frame designated by frame_id
    int LoadFrame(int frame_id);

    float getXAspect() const;
    float getYAspect() const;

    std::vector<uint8_t> getCurrentFrame() const {return this->media->getData();};
    std::string getFrameID(int frame) {return this->media->GetFrameID(frame);}; // This should be in data / time object

    //Can be removed when other objects have separate interface to media
    int getMediaHeight() const {return this->media->getHeight();};
    int getMediaWidth() const {return this->media->getWidth();};

protected:

    void mousePressEvent(QGraphicsSceneMouseEvent *event);
    void mouseReleaseEvent(QGraphicsSceneMouseEvent *event);
    void mouseMoveEvent(QGraphicsSceneMouseEvent *event);

    std::shared_ptr<MediaData> media;

private:
    QImage _mediaImage;

    QGraphicsPixmapItem* _canvasPixmap;
    QImage _canvasImage;
    int _canvasHeight;
    int _canvasWidth;

    QVector<QGraphicsPathItem*> _line_paths;
    QVector<QGraphicsEllipseItem*> _points;

    bool _is_verbose;

signals:
    void leftClick(qreal,qreal);
};

#endif // MEDIA_WINDOW_H
