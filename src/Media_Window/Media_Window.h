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
        this->points.append(addEllipse(static_cast<float>(x_canvas) * xAspect - radius/2,
                                       static_cast<float>(y_canvas) * yAspect - radius/2,
                                       radius, radius,color));
    }

    void clearPoints();

    void UpdateCanvas(QImage& img);

    std::vector<uint8_t> getCurrentFrame() const;

    int LoadMedia(std::string name);

    // Advance from current frame by num_frames
    int AdvanceFrame(int num_frames);

    //Jump to specific frame designated by frame_id
    int LoadFrame(int frame_id);

    int getLastLoadedFrame() const;

    int findNearestSnapFrame(int frame) const;
    std::string getFrameID(int frame);

    std::pair<int,int> getMediaDimensions() const;

    float getXAspect() const;
    float getYAspect() const;

protected:

    void mousePressEvent(QGraphicsSceneMouseEvent *event);
    void mouseReleaseEvent(QGraphicsSceneMouseEvent *event);
    void mouseMoveEvent(QGraphicsSceneMouseEvent *event);

    QImage myimage;
    std::vector<uint8_t> current_frame;
    QGraphicsPixmapItem* pixmap_item;
    int canvasHeight;
    int canvasWidth;

    QVector<QGraphicsPathItem*> line_paths;
    QVector<QGraphicsEllipseItem*> points;

    std::string vid_name; // Name of the video file

    int last_loaded_frame;
    int total_frame_count;

    virtual int doLoadMedia(std::string name) {return 0;};
    virtual int doLoadFrame(int frame_id) {return 0;};
    virtual int doFindNearestSnapFrame(int frame_id) const {return frame_id;};
    virtual std::string doGetFrameID(int frame_id) {return "";};
    virtual std::pair<int,int> doGetMediaDimensions() const {return std::pair<int,int>{0,0};};

    bool verbose_frame;


signals:
    void leftClick(qreal,qreal);
};


#endif // MEDIA_WINDOW_H
