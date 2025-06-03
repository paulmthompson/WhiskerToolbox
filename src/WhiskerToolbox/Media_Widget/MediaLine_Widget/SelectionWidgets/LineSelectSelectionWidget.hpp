#ifndef WHISKER_TOOLBOX_LINE_SELECT_SELECTION_WIDGET_HPP
#define WHISKER_TOOLBOX_LINE_SELECT_SELECTION_WIDGET_HPP

#include <QWidget>

namespace Ui {
class LineSelectSelectionWidget;
}

namespace line_widget {

/**
 * @brief Widget displayed when line selection mode is active
 * 
 * This widget provides controls for line selection including
 * selection threshold and information about the current selection.
 */
class LineSelectSelectionWidget : public QWidget {
    Q_OBJECT

public:
    /**
     * @brief Constructor
     * @param parent The parent widget
     */
    explicit LineSelectSelectionWidget(QWidget* parent = nullptr);
    
    /**
     * @brief Destructor
     */
    ~LineSelectSelectionWidget();

    /**
     * @brief Get the current selection threshold
     * @return Selection threshold in pixels
     */
    float getSelectionThreshold() const;

    /**
     * @brief Set the selection threshold
     * @param threshold Selection threshold in pixels
     */
    void setSelectionThreshold(float threshold);

signals:
    /**
     * @brief Emitted when the selection threshold changes
     * @param threshold New threshold value
     */
    void selectionThresholdChanged(float threshold);

private slots:
    void _onThresholdChanged(int threshold);

private:
    Ui::LineSelectSelectionWidget* ui;
};

} // namespace line_widget

#endif // WHISKER_TOOLBOX_LINE_SELECT_SELECTION_WIDGET_HPP 