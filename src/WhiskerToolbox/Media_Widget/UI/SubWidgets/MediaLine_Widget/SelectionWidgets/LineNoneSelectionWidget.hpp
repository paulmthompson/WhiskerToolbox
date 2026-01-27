#ifndef WHISKER_TOOLBOX_LINE_NONE_SELECTION_WIDGET_HPP
#define WHISKER_TOOLBOX_LINE_NONE_SELECTION_WIDGET_HPP

#include <QWidget>

namespace Ui {
class LineNoneSelectionWidget;
}

namespace line_widget {

/**
 * @brief Widget displayed when no line selection mode is active
 * 
 * This widget provides information to the user that no selection mode
 * is currently active and that clicking in the video will navigate.
 */
class LineNoneSelectionWidget : public QWidget {
    Q_OBJECT

public:
    /**
     * @brief Constructor
     * @param parent The parent widget
     */
    explicit LineNoneSelectionWidget(QWidget* parent = nullptr);
    
    /**
     * @brief Destructor
     */
    ~LineNoneSelectionWidget();

private:
    Ui::LineNoneSelectionWidget* ui;
};

} // namespace line_widget

#endif // WHISKER_TOOLBOX_LINE_NONE_SELECTION_WIDGET_HPP 