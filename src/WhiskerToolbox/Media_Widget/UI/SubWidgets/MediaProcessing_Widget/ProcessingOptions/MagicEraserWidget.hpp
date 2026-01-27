#ifndef MAGIC_ERASER_WIDGET_HPP
#define MAGIC_ERASER_WIDGET_HPP

#include "ImageProcessing/ProcessingOptions.hpp"

#include <QWidget>

namespace Ui {
class MagicEraserWidget;
}

/**
 * @brief Widget for controlling magic eraser tool options
 * 
 * This widget provides UI controls for adjusting brush size and median filter parameters
 * for the magic eraser functionality. The magic eraser replaces brush strokes with
 * median filtered content from the underlying image.
 */
class MagicEraserWidget : public QWidget {
    Q_OBJECT

public:
    explicit MagicEraserWidget(QWidget* parent = nullptr);
    ~MagicEraserWidget() override;

    /**
     * @brief Get the current magic eraser options
     * @return Current MagicEraserOptions structure
     */
    MagicEraserOptions getOptions() const;

    /**
     * @brief Set the magic eraser options and update UI controls
     * @param options MagicEraserOptions to apply to the widget
     */
    void setOptions(MagicEraserOptions const& options);

signals:
    /**
     * @brief Emitted when any magic eraser option changes
     * @param options Updated MagicEraserOptions structure
     */
    void optionsChanged(MagicEraserOptions const& options);

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
    void _onActiveChanged();
    void _onBrushSizeChanged();
    void _onMedianFilterSizeChanged();
    void _onDrawingModeChanged();
    void _onClearMaskClicked();

private:
    Ui::MagicEraserWidget* ui;

    void _updateOptions();
    void _blockSignalsAndSetValues(MagicEraserOptions const& options);
};

#endif // MAGIC_ERASER_WIDGET_HPP 