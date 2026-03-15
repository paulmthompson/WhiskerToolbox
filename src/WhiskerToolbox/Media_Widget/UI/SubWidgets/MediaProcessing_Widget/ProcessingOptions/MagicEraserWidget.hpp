#ifndef MAGIC_ERASER_WIDGET_HPP
#define MAGIC_ERASER_WIDGET_HPP

#include <QWidget>

class QPushButton;
class QLabel;
class QVBoxLayout;

/**
 * @brief Widget for magic eraser drawing controls
 *
 * Only manages drawing mode toggle and mask clearing. Parameter editing
 * (brush size, filter size) is handled by an external AutoParamWidget.
 */
class MagicEraserWidget : public QWidget {
    Q_OBJECT

public:
    explicit MagicEraserWidget(QWidget * parent = nullptr);

    [[nodiscard]] bool isDrawingMode() const;
    void setDrawingMode(bool enabled);

signals:
    /**
     * @brief Emitted when drawing mode is toggled
     * @param enabled True if drawing mode is active
     */
    void drawingModeChanged(bool enabled);

    /**
     * @brief Emitted when the user wants to clear the magic eraser mask
     */
    void clearMaskRequested();

private slots:
    void _onDrawingModeChanged();
    void _onClearMaskClicked();

private:
    QPushButton * _drawing_mode_button;
    QPushButton * _clear_mask_button;
};

#endif// MAGIC_ERASER_WIDGET_HPP