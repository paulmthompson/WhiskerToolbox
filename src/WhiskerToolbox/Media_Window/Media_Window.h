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
#include "Media_Data.h"


/**
 * The Media_Window class is responsible for plotting images, movies, and shapes on top of them.
 * Shapes may take the form of lines, points, or arbitrary 2d masks.
 * Advancing a frame will result in the video window loading new data.
 *
 */
class Media_Window : public QGraphicsScene

{
Q_OBJECT
public:
    Media_Window(QObject *parent = 0);


    /**
     *
     *
     * @tparam T
     * @param x
     * @param y
     * @param color
     */
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

    /**
     *
     *
     *
     * @tparam T
     * @param x_canvas
     * @param y_canvas
     * @param color
     * @param radius
     */
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

    /**
     *
     * @param img
     */
    void UpdateCanvas(QImage& img);

    int LoadMedia(std::string name);

    /**
     *
     * Jump to specific frame designated by frame_id
     *
     * @param frame_id
     * @return
     */
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

    QImage::Format _getQImageFormat();
    void _createCanvasForData();

signals:
    void leftClick(qreal,qreal);
};

#endif // MEDIA_WINDOW_H
