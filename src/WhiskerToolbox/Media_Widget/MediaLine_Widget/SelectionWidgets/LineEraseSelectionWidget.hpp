#ifndef WHISKER_TOOLBOX_LINE_ERASE_SELECTION_WIDGET_HPP
#define WHISKER_TOOLBOX_LINE_ERASE_SELECTION_WIDGET_HPP

#include <QWidget>

namespace Ui {
class LineEraseSelectionWidget;
}

namespace line_widget {

/**
 * @brief Widget for the "Erase Points" selection mode
 * 
 * This widget provides UI for erasing points from lines, including options for:
 * - Eraser radius adjustment
 */
class LineEraseSelectionWidget : public QWidget {
    Q_OBJECT

public:
    /**
     * @brief Constructor
     * @param parent The parent widget
     */
    explicit LineEraseSelectionWidget(QWidget* parent = nullptr);
    
    /**
     * @brief Destructor
     */
    ~LineEraseSelectionWidget();

    /**
     * @brief Get the current eraser radius
     * @return The current eraser radius value
     */
    int getEraserRadius() const;

    /**
     * @brief Set the eraser radius
     * @param radius The new radius value
     */
    void setEraserRadius(int radius);

signals:
    /**
     * @brief Signal emitted when eraser radius is changed
     * @param radius The new radius value
     */
    void eraserRadiusChanged(int radius);
    
    /**
     * @brief Signal emitted when the show circle checkbox is toggled
     * @param checked Whether the circle should be shown
     */
    void showCircleToggled(bool checked);

private:
    Ui::LineEraseSelectionWidget* ui;
    int _eraserRadius = 10; // Default eraser radius
};

} // namespace line_widget

#endif // WHISKER_TOOLBOX_LINE_ERASE_SELECTION_WIDGET_HPP 