#ifndef MEDIA_WINDOW_HPP
#define MEDIA_WINDOW_HPP

#include "CoreGeometry/ImageSize.hpp"
#include "CoreGeometry/masks.hpp"
#include "Media_Widget/DisplayOptions/CoordinateTypes.hpp"
#include "Media_Widget/DisplayOptions/DisplayOptions.hpp"

#include <QGraphicsEllipseItem>
#include <QGraphicsItem>
#include <QGraphicsScene>
#include <QGraphicsPathItem>
#include <QGraphicsRectItem>
#include <QGraphicsTextItem>
#include <QVector>
#include <QPointF>
#include <QtCore/QtGlobal>

#include <memory>
#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

class DataManager;
class TimeFrame;
class QGraphicsPixmapItem;
class QImage;
class MediaMask_Widget;
class MediaText_Widget;
class QGraphicsSceneContextMenuEvent;

struct TextOverlay; // Forward declare the TextOverlay struct

int const default_width = 640;
int const default_height = 480;

class Media_Window : public QGraphicsScene {
    Q_OBJECT
    friend class MediaMask_Widget;
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

    void setTextWidget(MediaText_Widget * text_widget);

    void UpdateCanvas();

    [[nodiscard]] float getXAspect() const;
    [[nodiscard]] float getYAspect() const;

    void setCanvasSize(ImageSize image_size) { _canvasWidth = image_size.width; _canvasHeight = image_size.height; }
    [[nodiscard]] std::pair<int,int> getCanvasSize() const { return {_canvasWidth,_canvasHeight}; }

    void setDrawingMode(bool drawing_mode) { _drawing_mode = drawing_mode; if(!drawing_mode) _drawing_points.clear(); }

    std::vector<uint8_t> getDrawingMask();

    void setShowHoverCircle(bool show);
    void setHoverCircleRadius(int radius);

    [[nodiscard]] std::optional<LineDisplayOptions*> getLineConfig(std::string const & line_key) const {
        if (_line_configs.find(line_key) == _line_configs.end()) return std::nullopt; return _line_configs.at(line_key).get(); }
    [[nodiscard]] std::optional<MaskDisplayOptions*> getMaskConfig(std::string const & mask_key) const {
        if (_mask_configs.find(mask_key) == _mask_configs.end()) return std::nullopt; return _mask_configs.at(mask_key).get(); }
    [[nodiscard]] std::optional<PointDisplayOptions*> getPointConfig(std::string const & point_key) const {
        if (_point_configs.find(point_key) == _point_configs.end()) return std::nullopt; return _point_configs.at(point_key).get(); }
    [[nodiscard]] std::optional<DigitalIntervalDisplayOptions*> getIntervalConfig(std::string const & interval_key) const {
        if (_interval_configs.find(interval_key) == _interval_configs.end()) return std::nullopt; return _interval_configs.at(interval_key).get(); }
    [[nodiscard]] std::optional<TensorDisplayOptions*> getTensorConfig(std::string const & tensor_key) const {
        if (_tensor_configs.find(tensor_key) == _tensor_configs.end()) return std::nullopt; return _tensor_configs.at(tensor_key).get(); }

    bool hasPreviewMaskData(std::string const & mask_key) const;
    std::vector<Mask2D> getPreviewMaskData(std::string const & mask_key) const;
    void setPreviewMaskData(std::string const & mask_key,
                            std::vector<std::vector<Point2D<uint32_t>>> const & preview_data,
                            bool active);

    // Snapshot utility
    QImage grabSnapshot() const;

signals:
    void leftClick(qreal, qreal);
    void rightClick(qreal, qreal);
    void leftClickMedia(qreal, qreal);
    void rightClickMedia(qreal, qreal);
    void leftRelease();
    void rightRelease();
    void leftReleaseDrawing();
    void rightReleaseDrawing();
    void canvasUpdated(QImage const & canvasImage);
    void mouseMove(qreal x, qreal y);
    void requestSnapshot();

    void leftClickCanvas(CanvasCoordinates const & coords);
    void rightClickCanvas(CanvasCoordinates const & coords);
    void leftClickMediaCoords(MediaCoordinates const & coords);
    void rightClickMediaCoords(MediaCoordinates const & coords);
    void mouseMoveCanvas(CanvasCoordinates const & coords);

public slots:
    void LoadFrame(int frame_id);

protected:
    void mousePressEvent(QGraphicsSceneMouseEvent * event) override;
    void mouseReleaseEvent(QGraphicsSceneMouseEvent * event) override;
    void mouseMoveEvent(QGraphicsSceneMouseEvent * event) override;
    void contextMenuEvent(QGraphicsSceneContextMenuEvent * event) override;

private:
    std::shared_ptr<DataManager> _data_manager;
    QImage _mediaImage;
    QGraphicsPixmapItem * _canvasPixmap = nullptr;
    QImage _canvasImage;
    int _canvasHeight{default_height};
    int _canvasWidth{default_width};

    QVector<QGraphicsPathItem*> _line_paths;
    QVector<QGraphicsItem*> _points;
    QVector<QGraphicsPixmapItem*> _masks;
    QVector<QGraphicsRectItem*> _mask_bounding_boxes;
    QVector<QGraphicsPathItem*> _mask_outlines;
    QVector<QGraphicsRectItem*> _intervals;
    QVector<QGraphicsPixmapItem*> _tensors;
    QVector<QGraphicsTextItem*> _text_items;

    bool _is_verbose{false};
    bool _debug_performance{false};
    bool _drawing_mode{false};
    bool _is_drawing{false};
    bool _show_hover_circle{false};
    int _hover_circle_radius{10};
    QPointF _hover_position;
    QGraphicsEllipseItem * _hover_circle_item{nullptr};

    std::vector<QPointF> _drawing_points;

    std::unordered_map<std::string, std::unique_ptr<LineDisplayOptions>> _line_configs;
    std::unordered_map<std::string, std::unique_ptr<MaskDisplayOptions>> _mask_configs;
    std::unordered_map<std::string, std::unique_ptr<PointDisplayOptions>> _point_configs;
    std::unordered_map<std::string, std::unique_ptr<DigitalIntervalDisplayOptions>> _interval_configs;
    std::unordered_map<std::string, std::unique_ptr<TensorDisplayOptions>> _tensor_configs;

    std::unordered_map<std::string, std::vector<std::vector<Point2D<uint32_t>>>> _preview_mask_data;
    bool _mask_preview_active{false};

    MediaText_Widget * _text_widget = nullptr;

    QImage::Format _getQImageFormat();
    void _createCanvasForData();
    void _convertNewMediaToQImage();

    void _plotLineData();
    void _clearLines();

    void _plotMaskData();
    void _clearMasks();
    void _clearMaskBoundingBoxes();
    void _clearMaskOutlines();
    void _plotSingleMaskData(std::vector<Mask2D> const & maskData, ImageSize mask_size, QRgb plot_color, MaskDisplayOptions const * mask_config);
    QImage _applyTransparencyMasks(QImage const & media_image);

    void _plotPointData();
    void _clearPoints();

    void _plotDigitalIntervalSeries();
    void _plotDigitalIntervalBorders();
    void _clearIntervals();

    void _plotTensorData();
    void _clearTensors();

    void _plotTextOverlays();
    void _clearTextOverlays();

    void _updateHoverCirclePosition();
    void _addRemoveData();
    bool _needsTimeFrameConversion(std::shared_ptr<TimeFrame> video_timeframe, const std::shared_ptr<TimeFrame>& interval_timeframe);
};

QRgb plot_color_with_alpha(BaseDisplayOptions const * opts);

#endif // MEDIA_WINDOW_HPP
