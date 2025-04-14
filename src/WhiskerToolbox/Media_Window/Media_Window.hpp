#ifndef MEDIA_WINDOW_HPP
#define MEDIA_WINDOW_HPP

#include "DataManager.hpp"
#include "DataManager/Masks/Mask_Data.hpp"
#include "DataManager/ImageSize/ImageSize.hpp"

#include <QGraphicsScene>
#include <QtCore/QtGlobal>

#include <memory>
#include <string>
#include <unordered_map>
#include <unordered_set>

class QGraphicsPixmapItem;
class QImage;

int const default_width = 640;
int const default_height = 480;

struct element_config {
    std::string hex_color;
    float alpha;
};

struct tensor_config {
    int channel;
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
class Media_Window : public QGraphicsScene {
    Q_OBJECT
public:
    explicit Media_Window(std::shared_ptr<DataManager> data_manager, QObject * parent = nullptr);


    void addLineDataToScene(
            std::string const & line_key,
            std::string const & hex_color = "#0000FF",
            float alpha = 1.0);

    void changeLineColor(std::string const & line_key, std::string const & hex_color);
    void changeLineAlpha(std::string const & line_key, float alpha);
    void removeLineDataFromScene(std::string const & line_key);
    void clearLines();

    void addMaskDataToScene(
            std::string const & mask_key,
            std::string const & hex_color = "#0000FF",
            float alpha = 1.0);

    void changeMaskColor(std::string const & mask_key, std::string const & hex_color);
    void changeMaskAlpha(float alpha);
    void changeMaskAlpha(std::string const & line_key, float alpha);
    void removeMaskDataFromScene(std::string const & mask_key);
    void clearMasks();

    void addPointDataToScene(
            std::string const & point_key,
            std::string const & hex_color = "#0000FF",
            float alpha = 1.0);

    void changePointColor(std::string const & point_key, std::string const & hex_color);
    void setPointAlpha(std::string const & point_key, float alpha);
    void removePointDataFromScene(std::string const & point_key);
    void clearPoints();

    void addDigitalIntervalSeries(
            std::string const & key,
            std::string const & hex_color = "#0000FF",
            float alpha = 1.0);

    void removeDigitalIntervalSeries(std::string const & key);
    void clearIntervals();

    void addTensorDataToScene(
            std::string const & tensor_key);
    void removeTensorDataFromScene(std::string const & tensor_key);
    void setTensorChannel(std::string const & tensor_key, int channel);
    void clearTensors();

    /**
     *
     *
     */
    void UpdateCanvas();

    [[nodiscard]] float getXAspect() const;
    [[nodiscard]] float getYAspect() const;

    void setCanvasSize(ImageSize image_size) {
        _canvasWidth = image_size.width;
        _canvasHeight = image_size.height;
    }

    [[nodiscard]] std::pair<int, int> getCanvasSize() const {
        return std::make_pair(_canvasWidth, _canvasHeight);
    }

    void setDrawingMode(bool drawing_mode) {
        _drawing_mode = drawing_mode;
        if (!drawing_mode) {
            _drawing_points.clear();
        }
    }

    std::vector<uint8_t> getDrawingMask();

protected:
    void mousePressEvent(QGraphicsSceneMouseEvent * event) override;
    void mouseReleaseEvent(QGraphicsSceneMouseEvent * event) override;
    void mouseMoveEvent(QGraphicsSceneMouseEvent * event) override;

private:
    std::shared_ptr<DataManager> _data_manager;

    QImage _mediaImage;
    QGraphicsPixmapItem * _canvasPixmap = nullptr;
    QImage _canvasImage;

    int _canvasHeight{default_height};
    int _canvasWidth{default_width};

    QVector<QGraphicsPathItem *> _line_paths;
    QVector<QGraphicsEllipseItem *> _points;
    QVector<QGraphicsPixmapItem *> _masks;
    QVector<QGraphicsRectItem *> _intervals;
    QVector<QGraphicsPixmapItem *> _tensors;

    bool _is_verbose{false};
    bool _drawing_mode{false};
    bool _is_drawing{false};

    std::vector<QPointF> _drawing_points;

    std::unordered_map<std::string, element_config> _line_configs;
    std::unordered_map<std::string, element_config> _mask_configs;
    std::unordered_map<std::string, element_config> _point_configs;
    std::unordered_map<std::string, element_config> _interval_configs;
    std::unordered_map<std::string, tensor_config> _tensor_configs;

    QImage::Format _getQImageFormat();
    void _createCanvasForData();
    void _convertNewMediaToQImage();
    void _plotLineData();
    void _plotMaskData();
    void _plotSingleMaskData(std::vector<Mask2D> const & maskData, int mask_width, int mask_height, QRgb plot_color);
    void _plotPointData();
    void _plotDigitalIntervalSeries();
    void _plotTensorData();

public slots:
    void LoadFrame(int frame_id);
signals:
    void leftClick(qreal, qreal);
    void leftClickMedia(qreal, qreal);
    void leftRelease();
    void canvasUpdated(QImage const & canvasImage);
};

QRgb plot_color_with_alpha(element_config const & elem);

#endif// MEDIA_WINDOW_HPP
