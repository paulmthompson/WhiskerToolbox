#ifndef MEDIATEXT_WIDGET_HPP
#define MEDIATEXT_WIDGET_HPP


#include <QColor>
#include <QString>
#include <QWidget>

#include <memory>
#include <vector>

class QAction;
class QMenu;
class QTableWidgetItem;

namespace Ui {
class MediaText_Widget;
}

enum class TextOrientation {
    Horizontal,
    Vertical
};

struct TextOverlay {
    QString text;
    TextOrientation orientation{TextOrientation::Horizontal};
    float x_position{0.5f};// Relative position 0.0-1.0
    float y_position{0.5f};// Relative position 0.0-1.0
    QColor color{Qt::white};
    int font_size{12};
    bool enabled{true};
    int id{-1};// Unique identifier for this overlay

    TextOverlay() = default;
    TextOverlay(QString const & text_content, TextOrientation orient = TextOrientation::Horizontal,
                float x = 0.5f, float y = 0.5f, QColor const & text_color = Qt::white,
                int size = 12, bool is_enabled = true)
        : text(text_content),
          orientation(orient),
          x_position(x),
          y_position(y),
          color(text_color),
          font_size(size),
          enabled(is_enabled) {}
};

class MediaText_Widget : public QWidget {
    Q_OBJECT

public:
    explicit MediaText_Widget(QWidget * parent = nullptr);
    ~MediaText_Widget() override;

    // Text overlay management
    void addTextOverlay(TextOverlay const & overlay);
    void removeTextOverlay(int overlay_id);
    void updateTextOverlay(int overlay_id, TextOverlay const & updated_overlay);
    void clearAllTextOverlays();

    // Data access
    std::vector<TextOverlay> const & getTextOverlays() const { return _text_overlays; }
    std::vector<TextOverlay> getEnabledTextOverlays() const;

    // Table management
    void refreshTable();

signals:
    void textOverlayAdded(TextOverlay const & overlay);
    void textOverlayRemoved(int overlay_id);
    void textOverlayUpdated(int overlay_id, TextOverlay const & overlay);
    void textOverlaysCleared();

private slots:
    void _onAddTextClicked();
    void _onTableItemChanged(QTableWidgetItem * item);
    void _onTableContextMenu(QPoint const & position);
    void _onDeleteSelectedOverlay();
    void _onToggleOverlayEnabled();
    void _onEditSelectedOverlay();

private:
    Ui::MediaText_Widget * ui;

    // Data storage
    std::vector<TextOverlay> _text_overlays;
    int _next_overlay_id{0};

    // Context menu
    std::unique_ptr<QMenu> _context_menu;
    QAction * _delete_action;
    QAction * _toggle_enabled_action;
    QAction * _edit_action;

    // Helper methods
    void _setupTable();
    void _setupContextMenu();
    void _populateTableRow(int row, TextOverlay const & overlay);
    void _updateTableRow(int row, TextOverlay const & overlay);
    int _findOverlayRowById(int overlay_id) const;
    int _getSelectedOverlayId() const;
    QString _orientationToString(TextOrientation orientation) const;
    TextOrientation _stringToOrientation(QString const & str) const;
    QString _colorToString(QColor const & color) const;
    QColor _stringToColor(QString const & str) const;
};

#endif// MEDIATEXT_WIDGET_HPP