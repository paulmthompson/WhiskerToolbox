#ifndef WHISKER_TOOLBOX_MASK_BRUSH_SELECTION_WIDGET_HPP
#define WHISKER_TOOLBOX_MASK_BRUSH_SELECTION_WIDGET_HPP

#include <QWidget>

namespace Ui {
class MaskBrushSelectionWidget;
}

namespace mask_widget {

/**
 * @brief Widget for the "Brush" selection mode
 * 
 * This widget provides UI for both adding to and erasing from mask areas:
 * - Left click adds to the mask
 * - Right click erases from the mask
 * - Brush size adjustment affects both operations
 * - Hover circle visibility can be toggled
 */
class MaskBrushSelectionWidget : public QWidget {
    Q_OBJECT

public:
    /**
     * @brief Constructor
     * @param parent The parent widget
     */
    explicit MaskBrushSelectionWidget(QWidget * parent = nullptr);

    /**
     * @brief Destructor
     */
    ~MaskBrushSelectionWidget();

    /**
     * @brief Get the current brush size
     * @return The current brush size value
     */
    int getBrushSize() const;

    /**
     * @brief Set the brush size
     * @param size The new size value
     */
    void setBrushSize(int size);

    /**
     * @brief Get the current hover circle visibility state
     * @return Whether the hover circle is visible
     */
    bool isHoverCircleVisible() const;

    /**
     * @brief Set the hover circle visibility
     * @param visible Whether the hover circle should be visible
     */
    void setHoverCircleVisible(bool visible);

    /**
     * @brief Get the current allow empty mask state
     * @return Whether empty masks should be preserved
     */
    bool isAllowEmptyMask() const;

    /**
     * @brief Set the allow empty mask state
     * @param allow Whether empty masks should be preserved
     */
    void setAllowEmptyMask(bool allow);

signals:
    /**
     * @brief Signal emitted when brush size is changed
     * @param size The new brush size value
     */
    void brushSizeChanged(int size);

    /**
     * @brief Signal emitted when hover circle visibility is toggled
     * @param visible Whether the hover circle should be visible
     */
    void hoverCircleVisibilityChanged(bool visible);

    /**
     * @brief Signal emitted when allow empty mask setting is toggled
     * @param allow Whether empty masks should be preserved
     */
    void allowEmptyMaskChanged(bool allow);

private:
    Ui::MaskBrushSelectionWidget * ui;
    int _brushSize = 15;            // Default brush size
    bool _hoverCircleVisible = true;// Default visibility
    bool _allowEmptyMask = false;   // Default: do not preserve empty masks
};

}// namespace mask_widget

#endif// WHISKER_TOOLBOX_MASK_BRUSH_SELECTION_WIDGET_HPP