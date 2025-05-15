#ifndef MEDIA_WINDOW_HPP
#define MEDIA_WINDOW_HPP

#include "DataManager.hpp"
#include "DataManager/Masks/Mask_Data.hpp"
#include "DataManager/ImageSize/ImageSize.hpp"

#include "Media_Widget/DisplayOptions/DisplayOptions.hpp"

#include <QGraphicsScene>
#include <QtCore/QtGlobal>

#include <memory>
#include <optional>
#include <string>
#include <unordered_map>
class QGraphicsPixmapItem;
class QImage;

int const default_width = 640;
int const default_height = 480;


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


    void addLineDataToScene(std::string const & line_key);
    void removeLineDataFromScene(std::string const & line_key);

    void addMaskDataToScene(std::string const & mask_key);
    void removeMaskDataFromScene(std::string const & mask_key);

    void addPointDataToScene(std::string const & point_key);
    void removePointDataFromScene(std::string const & point_key);

    void addDigitalIntervalSeries(std::string const & key);
    void removeDigitalIntervalSeries(std::string const & key);

    void addTensorDataToScene(std::string const & tensor_key);
    void removeTensorDataFromScene(std::string const & tensor_key);

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

    void setShowHoverCircle(bool show);
    void setHoverCircleRadius(int radius);

    [[nodiscard]] std::optional<LineDisplayOptions *> getLineConfig(std::string const & line_key) const {
        if (_line_configs.find(line_key) == _line_configs.end()) {
            return std::nullopt;
        }
        return _line_configs.at(line_key).get();
    }

    [[nodiscard]] std::optional<MaskDisplayOptions *> getMaskConfig(std::string const & mask_key) const {
        if (_mask_configs.find(mask_key) == _mask_configs.end()) {
            return std::nullopt;
        }
        return _mask_configs.at(mask_key).get();
    }

    [[nodiscard]] std::optional<PointDisplayOptions *> getPointConfig(std::string const & point_key) const {
        if (_point_configs.find(point_key) == _point_configs.end()) {
            return std::nullopt;
        }
        return _point_configs.at(point_key).get();
    }

    [[nodiscard]] std::optional<DigitalIntervalDisplayOptions *> getIntervalConfig(std::string const & interval_key) const {
        if (_interval_configs.find(interval_key) == _interval_configs.end()) {
            return std::nullopt;
        }
        return _interval_configs.at(interval_key).get();
    }

    [[nodiscard]] std::optional<TensorDisplayOptions *> getTensorConfig(std::string const & tensor_key) const {
        if (_tensor_configs.find(tensor_key) == _tensor_configs.end()) {
            return std::nullopt;
        }
        return _tensor_configs.at(tensor_key).get();
    }

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
    bool _show_hover_circle {false};
    int _hover_circle_radius {10};
    QPointF _hover_position;

    std::vector<QPointF> _drawing_points;

    std::unordered_map<std::string, std::unique_ptr<LineDisplayOptions>> _line_configs;
    std::unordered_map<std::string, std::unique_ptr<MaskDisplayOptions>> _mask_configs;
    std::unordered_map<std::string, std::unique_ptr<PointDisplayOptions>> _point_configs;
    std::unordered_map<std::string, std::unique_ptr<DigitalIntervalDisplayOptions>> _interval_configs;
    std::unordered_map<std::string, std::unique_ptr<TensorDisplayOptions>> _tensor_configs;

    QImage::Format _getQImageFormat();
    void _createCanvasForData();
    void _convertNewMediaToQImage();

    void _plotLineData();
    void _clearLines();

    void _plotMaskData();
    void _clearMasks();
    void _plotSingleMaskData(std::vector<Mask2D> const & maskData, ImageSize mask_size, QRgb plot_color);

    void _plotPointData();
    void _clearPoints();

    void _plotDigitalIntervalSeries();
    void _clearIntervals();

    void _plotTensorData();
    void _clearTensors();

    void _plotHoverCircle();

public slots:
    void LoadFrame(int frame_id);
signals:
    void leftClick(qreal, qreal);
    void leftClickMedia(qreal, qreal);
    void leftRelease();
    void canvasUpdated(QImage const & canvasImage);
    void mouseMove(qreal x, qreal y);
};

QRgb plot_color_with_alpha(BaseDisplayOptions const * opts);

#endif// MEDIA_WINDOW_HPP
