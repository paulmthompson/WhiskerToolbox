#ifndef MEDIA_WINDOW_HPP
#define MEDIA_WINDOW_HPP

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

    void addMaskDataToScene(const std::string& mask_key);
    void addMaskColor(std::string const& mask_key, QColor const color );

    void clearMasks();

    void setMaskAlpha(int const alpha) {_mask_alpha = alpha; UpdateCanvas();};

    void addPointDataToScene(const std::string& point_key);
    void addPointColor(std::string const& point_key, QColor const color);
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
    QVector<QGraphicsPixmapItem*> _masks;

    int _mask_alpha;

    bool _is_verbose;

    std::unordered_set<std::string> _lines_to_show;
    std::unordered_map<std::string,QColor> _line_colors;

    std::unordered_set<std::string> _masks_to_show;
    std::unordered_map<std::string,QColor> _mask_colors;

    std::unordered_set<std::string> _points_to_show;
    std::unordered_map<std::string,QColor> _point_colors;

    QImage::Format _getQImageFormat();
    void _createCanvasForData();
    void _convertNewMediaToQImage();
    void _plotLineData();
    void _plotMaskData();
    void _plotSingleMaskData(std::vector<Mask2D> const & maskData, int const mask_width, int const mask_height, QRgb plot_color);
    QRgb _create_mask_plot_color(std::string const& mask_key);

public slots:
    void LoadFrame(int frame_id);
signals:
    void leftClick(qreal,qreal);
};

#endif // MEDIA_WINDOW_HPP
