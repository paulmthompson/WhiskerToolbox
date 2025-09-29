#ifndef MEDIA_WINDOW_HPP
#define MEDIA_WINDOW_HPP

#include "../Media_Widget/DisplayOptions/CoordinateTypes.hpp"
#include "../Media_Widget/DisplayOptions/DisplayOptions.hpp"
#include "CoreGeometry/ImageSize.hpp"
#include "CoreGeometry/masks.hpp"
#include "DataManager/Entity/EntityTypes.hpp"

#include <QGraphicsEllipseItem>
#include <QGraphicsItem>
#include <QGraphicsScene>
#include <QMenu>
#include <QtCore/QtGlobal>

#include <memory>
#include <optional>
#include <set>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

class DataManager;
class TimeFrame;
class QGraphicsPixmapItem;
class QImage;
class MediaMask_Widget;
class MediaText_Widget;
class Media_Widget;
class GroupManager;
class LineData;
class PointData;
class MaskData;

struct TextOverlay;

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

    friend class MediaMask_Widget;// Allow access to preview functionality

public:
    explicit Media_Window(std::shared_ptr<DataManager> data_manager, QObject * parent = nullptr);

    ~Media_Window() override;

    void addMediaDataToScene(std::string const & media_key);
    void removeMediaDataFromScene(std::string const & media_key);

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

    // Text overlay methods
    void setTextWidget(MediaText_Widget * text_widget);

    // Parent widget access for enabled media keys
    void setParentWidget(QWidget * parent_widget) {
        _parent_widget = parent_widget;
    }

    /**
     * @brief Set the GroupManager for group-aware plotting
     * @param group_manager Pointer to the GroupManager instance
     */
    void setGroupManager(GroupManager * group_manager);

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

    // Temporary line visualization for drawing modes
    void setShowTemporaryLine(bool show);
    void updateTemporaryLine(std::vector<Point2D<float>> const & points, std::string const & line_key = "");
    void clearTemporaryLine();

    // Selection and context menu functionality
    void clearAllSelections();
    bool hasSelections() const;
    std::unordered_set<EntityId> getSelectedEntities() const;
    void selectEntity(EntityId entity_id, std::string const & data_key, std::string const & data_type);
    
    // Selection mode coordination
    void setGroupSelectionEnabled(bool enabled);
    bool isGroupSelectionEnabled() const;
    
    // Public methods for entity finding
    EntityId findPointAtPosition(QPointF const & scene_pos, std::string const & point_key);
    EntityId findEntityAtPosition(QPointF const & scene_pos, std::string & data_key, std::string & data_type);

    [[nodiscard]] std::optional<MediaDisplayOptions *> getMediaConfig(std::string const & media_key) const {
        if (_media_configs.find(media_key) == _media_configs.end()) {
            return std::nullopt;
        }
        return _media_configs.at(media_key).get();
    }

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

    /**
     * @brief Check if preview mask data is available for a given key
     * @param mask_key The mask key to check
     * @return True if preview data exists for this key
     */
    [[nodiscard]] bool hasPreviewMaskData(std::string const & mask_key) const;

    /**
     * @brief Get preview mask data for a given key and time
     * @param mask_key The mask key
     * @return Preview mask data if available, empty vector otherwise
     */
    [[nodiscard]] std::vector<Mask2D> getPreviewMaskData(std::string const & mask_key) const;

    /**
     * @brief Set preview mask data for a given key
     * @param mask_key The mask key
     * @param preview_data The preview mask data
     * @param active Whether preview is active
     */
    void setPreviewMaskData(std::string const & mask_key,
                            std::vector<std::vector<Point2D<uint32_t>>> const & preview_data,
                            bool active);

protected:
    void mousePressEvent(QGraphicsSceneMouseEvent * event) override;
    void mouseReleaseEvent(QGraphicsSceneMouseEvent * event) override;
    void mouseMoveEvent(QGraphicsSceneMouseEvent * event) override;
    void contextMenuEvent(QGraphicsSceneContextMenuEvent * event) override;

private:
    std::shared_ptr<DataManager> _data_manager;
    QWidget * _parent_widget = nullptr;
    GroupManager * _group_manager = nullptr;

    QGraphicsPixmapItem * _canvasPixmap = nullptr;
    QImage _canvasImage;

    int _canvasHeight{default_height};
    int _canvasWidth{default_width};

    QVector<QGraphicsPathItem *> _line_paths;
    QVector<QGraphicsItem *> _points;
    QVector<QGraphicsPixmapItem *> _masks;
    QVector<QGraphicsRectItem *> _mask_bounding_boxes;
    QVector<QGraphicsPathItem *> _mask_outlines;
    QVector<QGraphicsRectItem *> _intervals;
    QVector<QGraphicsPixmapItem *> _tensors;
    QVector<QGraphicsTextItem *> _text_items;// Text overlay items

    bool _is_verbose{false};
    bool _debug_performance{false};// Debug flag for performance-related output
    bool _drawing_mode{false};
    bool _is_drawing{false};
    bool _show_hover_circle{false};
    int _hover_circle_radius{10};
    QPointF _hover_position;
    QGraphicsEllipseItem * _hover_circle_item{nullptr};// Track hover circle item

    // Temporary line visualization
    bool _show_temporary_line{false};
    QGraphicsPathItem * _temporary_line_item{nullptr};// Track temporary line item
    std::vector<QGraphicsEllipseItem *> _temporary_line_points;// Track temporary line point markers

    std::vector<QPointF> _drawing_points;

    std::unordered_map<std::string, std::unique_ptr<MediaDisplayOptions>> _media_configs;
    std::unordered_map<std::string, std::unique_ptr<LineDisplayOptions>> _line_configs;
    std::unordered_map<std::string, std::unique_ptr<MaskDisplayOptions>> _mask_configs;
    std::unordered_map<std::string, std::unique_ptr<PointDisplayOptions>> _point_configs;
    std::unordered_map<std::string, std::unique_ptr<DigitalIntervalDisplayOptions>> _interval_configs;
    std::unordered_map<std::string, std::unique_ptr<TensorDisplayOptions>> _tensor_configs;

    // Preview data storage for masks
    std::unordered_map<std::string, std::vector<std::vector<Point2D<uint32_t>>>> _preview_mask_data;
    bool _mask_preview_active{false};

    // Text overlay support
    MediaText_Widget * _text_widget = nullptr;

    // Selection and context menu support
    std::unordered_set<EntityId> _selected_entities;
    std::string _selected_data_key;  // Key of the data containing selected entities
    std::string _selected_data_type; // Type of selected data ("line", "point", "mask")
    bool _group_selection_enabled = true; // Allow group-based selection to be disabled
    QMenu * _context_menu = nullptr;

    QImage::Format _getQImageFormat(std::string const & media_key);
    QImage _combineMultipleMedia();

    void _plotMediaData();
    void _clearMedia();

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

    // Helper for timeframe conversion
    bool _needsTimeFrameConversion(std::shared_ptr<TimeFrame> video_timeframe,
                                   std::shared_ptr<TimeFrame> const & interval_timeframe);
    
    // Group-aware color helpers
    [[nodiscard]] QColor _getGroupAwareColor(EntityId entity_id, QColor const & default_color) const;
    [[nodiscard]] QRgb _getGroupAwareColorRgb(EntityId entity_id, QRgb default_color) const;

    // Selection and context menu helpers
    EntityId _findEntityAtPosition(QPointF const & scene_pos, std::string & data_key, std::string & data_type);
    EntityId _findLineAtPosition(QPointF const & scene_pos, std::string const & line_key);
    EntityId _findPointAtPosition(QPointF const & scene_pos, std::string const & point_key);
    EntityId _findMaskAtPosition(QPointF const & scene_pos, std::string const & mask_key);
    void _createContextMenu();
    void _showContextMenu(QPoint const & global_pos);
    void _updateContextMenuActions();
    float _calculateDistanceToLineSegment(float px, float py, float x1, float y1, float x2, float y2);

public slots:
    void LoadFrame(int frame_id);

private slots:
    /**
     * @brief Handle when a group is created, modified, or removed
     * Updates the canvas to reflect color changes
     */
    void onGroupChanged();

    // Context menu actions
    void onCreateNewGroup();
    void onAssignToGroup(int group_id);
    void onUngroupSelected();
    void onClearSelection();

signals:
    void leftClick(qreal, qreal);
    void rightClick(qreal, qreal);
    void leftClickMedia(qreal, qreal);
    void rightClickMedia(qreal, qreal);
    void leftRelease();
    void rightRelease();
    void leftReleaseDrawing(); // Only emitted when in drawing mode
    void rightReleaseDrawing();// Only emitted when in drawing mode
    void canvasUpdated(QImage const & canvasImage);
    void mouseMove(qreal x, qreal y);

    // Strong-typed coordinate signals
    void leftClickCanvas(CanvasCoordinates const & coords);
    void rightClickCanvas(CanvasCoordinates const & coords);
    void leftClickMediaCoords(MediaCoordinates const & coords);
    void rightClickMediaCoords(MediaCoordinates const & coords);
    void mouseMoveCanvas(CanvasCoordinates const & coords);
    
    // Mouse event signal with full event information (for modifier detection)
    void leftClickMediaWithEvent(qreal x_media, qreal y_media, Qt::KeyboardModifiers modifiers);
};

QRgb plot_color_with_alpha(BaseDisplayOptions const * opts);

#endif// MEDIA_WINDOW_HPP
