#ifndef VIDEO_WINDOW_H
#define VIDEO_WINDOW_H

#include <QGraphicsScene>
#include <QGraphicsSceneMouseEvent>
#include <QGraphicsPixmapItem>
#include <QImage>
#include <ffmpeg_wrapper/videodecoder.h>

/*

The Video_Window class

*/

class Video_Window : public QGraphicsScene
{
Q_OBJECT
public:
    Video_Window(QObject *parent = 0);

    //Template member functions must be defined in header file so that compiler than create specialized versions
    //Other methods can achieve this
    template <typename T>
    void addLine(std::vector<T>& x, std::vector<T>& y, QPen color) {
        QPainterPath* path = new QPainterPath();

        path->moveTo(QPointF(x[0],y[0]));

        for (int i = 1; i < x.size(); i++) {
            path->lineTo(QPointF(x[i],y[i]));
        }

        addLine(path,color);
    }

    void addLine(QPainterPath* path, QPen color);

    void clearLines();

    template <typename T>
    void addPoint(T x, T y, QPen color);

    void clearPoints();

    void UpdateCanvas();

    void UpdateCanvas(QImage& img);

    std::vector<uint8_t> getCurrentFrame() const;

    int GetVideoInfo(std::string name);

    // Advance from current frame by num_frames
    int AdvanceFrame(int num_frames);

    //Jump to specific frame designated by frame_id
    int LoadFrame(int frame_id,bool frame_by_frame = false);

    int getLastLoadedFrame() const;

    int findNearestKeyframe(int frame) const;

protected:

    void mousePressEvent(QGraphicsSceneMouseEvent *event);
    void mouseReleaseEvent(QGraphicsSceneMouseEvent *event);
    void mouseMoveEvent(QGraphicsSceneMouseEvent *event);

    std::string vid_name;
    QImage myimage;
    std::vector<uint8_t> current_frame;
    QGraphicsPixmapItem* pixmap_item;
    int h;
    int w;

    QVector<QGraphicsPathItem*> line_paths;
    QVector<QGraphicsEllipseItem*> points;

    std::unique_ptr<ffmpeg_wrapper::VideoDecoder> vd;
    int last_loaded_frame;


signals:
    void leftClick(qreal,qreal);
};


#endif // VIDEO_WINDOW_H
