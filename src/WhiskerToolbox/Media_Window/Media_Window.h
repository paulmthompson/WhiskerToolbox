#ifndef MEDIA_WINDOW_H
#define MEDIA_WINDOW_H

#include <QGraphicsScene>
#include <QGraphicsSceneMouseEvent>
#include <QGraphicsPixmapItem>
#include <QImage>
#include <QtCore/QtGlobal>


#include <memory>
#include <string>
#include <unordered_set>
#include <unordered_map>

#include "DataManager.hpp"


#if defined _WIN32 || defined __CYGWIN__
    #define MEDIA_WINDOW_DLLOPT
//  #define MEDIA_WINDOW_DLLOPT Q_DECL_EXPORT
#else
    #define MEDIA_WINDOW_DLLOPT __attribute__((visibility("default")))
#endif


/**
 * The Media_Window class is responsible for plotting images, movies, and shapes on top of them.
 * Shapes may take the form of lines, points, or arbitrary 2d masks.
 * Advancing a frame will result in the video window loading new data.
 *
 * This is a QGraphicsScene, which internally renders lines, paths, points, and
 * shapes which are added to the scene
 *
 * Shapes can be added to the specific frame being visualized after it has been rendered and saved
 * or just temporarily (scrolled back it will not be there), or data assets can be loaded which are
 * saved for the duration (loading keypoints to be plotted with each corresponding frame).
 *
 */
class MEDIA_WINDOW_DLLOPT Media_Window : public QGraphicsScene
{
Q_OBJECT
public:
Media_Window(std::shared_ptr<DataManager> data_manager, QObject *parent = 0);


    void addLineDataToScene(const std::string line_key);
    void addLineColor(std::string line_key, const QColor color);

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
     *
     */
    void UpdateCanvas();

    float getXAspect() const;
    float getYAspect() const;

protected:

    void mousePressEvent(QGraphicsSceneMouseEvent *event) override;
    void mouseReleaseEvent(QGraphicsSceneMouseEvent *event) override;
    void mouseMoveEvent(QGraphicsSceneMouseEvent *event) override;

private:
    std::shared_ptr<DataManager> _data_manager;

    QImage _mediaImage;
    QGraphicsPixmapItem* _canvasPixmap;
    QImage _canvasImage;

    int _canvasHeight;
    int _canvasWidth;


    QVector<QGraphicsPathItem*> _line_paths;
    QVector<QGraphicsEllipseItem*> _points;

    bool _is_verbose;

    std::unordered_set<std::string> _lines_to_show;
    std::unordered_map<std::string,QColor> _line_colors;

    QImage::Format _getQImageFormat();
    void _createCanvasForData();
    void _convertNewMediaToQImage();
    void _plotLineData();

signals:
    void leftClick(qreal,qreal);
};

#endif // MEDIA_WINDOW_H
