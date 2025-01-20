#ifndef MEDIA_WINDOW_HPP
#define MEDIA_WINDOW_HPP

#include "DataManager.hpp"
#include "DataManager/Masks/Mask_Data.hpp"

#include <QGraphicsScene>
#include <QtCore/QtGlobal>

#include <memory>
#include <string>
#include <unordered_set>
#include <unordered_map>

class QGraphicsPixmapItem;
class QImage;

int const default_width = 640;
int const default_height = 480;

struct element_config {
    std::string hex_color;
    float alpha;
};


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
class Media_Window : public QGraphicsScene
{
Q_OBJECT
public:
Media_Window(std::shared_ptr<DataManager> data_manager, QObject *parent = 0);


    void addLineDataToScene(
            std::string const & line_key,
            std::string const & hex_color = "#0000FF",
            float alpha = 1.0);

    void changeLineColor(std::string const & line_key, std::string const & hex_color);
    void changeLineAlpha(std::string const & line_key, float const alpha);
    void removeLineDataFromScene(std::string const & line_key);
    void clearLines();

    void addMaskDataToScene(
            const std::string& mask_key,
            std::string const & hex_color = "#0000FF",
            float alpha = 1.0);

    void changeMaskColor(std::string const & mask_key, std::string const & hex_color);
    void changeMaskAlpha(float const alpha);
    void changeMaskAlpha(std::string const & line_key, float const alpha);
    void removeMaskDataFromScene(std::string const & mask_key);
    void clearMasks();

    void addPointDataToScene(
            const std::string& point_key,
            std::string const & hex_color = "#0000FF",
            float alpha = 1.0);

    void changePointColor(std::string const & point_key, std::string const & hex_color);
    void setPointAlpha(std::string const & point_key, float const alpha);
    void removePointDataFromScene(std::string const & point_key);
    void clearPoints();

    /**
     *
     *
     */
    void UpdateCanvas();

    float getXAspect() const;
    float getYAspect() const;

    void setCanvasSize(int width, int height) {
        _canvasWidth = width;
        _canvasHeight = height;
    }

    void setDrawingMode(bool drawing_mode)
        {_drawing_mode = drawing_mode;
        if (!drawing_mode)
        {
            _drawing_points.clear();
        }
        };

    std::vector<uint8_t> getDrawingMask();

protected:

    void mousePressEvent(QGraphicsSceneMouseEvent *event) override;
    void mouseReleaseEvent(QGraphicsSceneMouseEvent *event) override;
    void mouseMoveEvent(QGraphicsSceneMouseEvent *event) override;

private:
    std::shared_ptr<DataManager> _data_manager;

    QImage _mediaImage;
    QGraphicsPixmapItem* _canvasPixmap;
    QImage _canvasImage;

    int _canvasHeight {default_height};
    int _canvasWidth {default_width};

    QVector<QGraphicsPathItem*> _line_paths;
    QVector<QGraphicsEllipseItem*> _points;
    QVector<QGraphicsPixmapItem*> _masks;

    bool _is_verbose {false};
    bool _drawing_mode {false};
    bool _is_drawing {false};

    std::vector<QPointF> _drawing_points;

    std::unordered_map<std::string, element_config> _line_configs;
    std::unordered_map<std::string, element_config> _mask_configs;
    std::unordered_map<std::string, element_config> _point_configs;

    QImage::Format _getQImageFormat();
    QRgb _plot_color_with_alpha(element_config elem);
    void _createCanvasForData();
    void _convertNewMediaToQImage();
    void _plotLineData();
    void _plotMaskData();
    void _plotSingleMaskData(std::vector<Mask2D> const & maskData, int const mask_width, int const mask_height, QRgb plot_color);
    void _plotPointData();

public slots:
    void LoadFrame(int frame_id);
signals:
    void leftClick(qreal,qreal);
    void leftRelease();
};

#endif // MEDIA_WINDOW_HPP
