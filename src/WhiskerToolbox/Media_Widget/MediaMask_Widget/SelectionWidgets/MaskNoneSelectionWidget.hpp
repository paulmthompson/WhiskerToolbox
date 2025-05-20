#ifndef WHISKER_TOOLBOX_MASK_NONE_SELECTION_WIDGET_HPP
#define WHISKER_TOOLBOX_MASK_NONE_SELECTION_WIDGET_HPP

#include <QWidget>

namespace Ui {
class MaskNoneSelectionWidget;
}

namespace mask_widget {

/**
 * @brief Widget displayed when no mask selection mode is active
 * 
 * This widget provides information to the user that no selection mode
 * is currently active and that clicking in the video will navigate.
 */
class MaskNoneSelectionWidget : public QWidget {
    Q_OBJECT

public:
    /**
     * @brief Constructor
     * @param parent The parent widget
     */
    explicit MaskNoneSelectionWidget(QWidget* parent = nullptr);
    
    /**
     * @brief Destructor
     */
    ~MaskNoneSelectionWidget();

private:
    Ui::MaskNoneSelectionWidget* ui;
};

} // namespace mask_widget

#endif // WHISKER_TOOLBOX_MASK_NONE_SELECTION_WIDGET_HPP 