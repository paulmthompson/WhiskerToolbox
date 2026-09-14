#ifndef WHISKER_TOOLBOX_LINE_SELECT_SELECTION_WIDGET_HPP
#define WHISKER_TOOLBOX_LINE_SELECT_SELECTION_WIDGET_HPP

#include <QWidget>

namespace Ui {
class LineSelectSelectionWidget;
}

namespace line_widget {

/**
 * @brief Widget displayed when line edit mode is active
 *
 * Provides instructions for editing a line that was selected with the global Select tool.
 */
class LineSelectSelectionWidget : public QWidget {
    Q_OBJECT

public:
    /**
     * @brief Constructor
     * @param parent The parent widget
     */
    explicit LineSelectSelectionWidget(QWidget * parent = nullptr);

    /**
     * @brief Destructor
     */
    ~LineSelectSelectionWidget() override;

private:
    Ui::LineSelectSelectionWidget * ui;
};

}// namespace line_widget

#endif// WHISKER_TOOLBOX_LINE_SELECT_SELECTION_WIDGET_HPP
