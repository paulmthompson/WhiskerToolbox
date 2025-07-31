#ifndef GLOBALPROPERTIESWIDGET_HPP
#define GLOBALPROPERTIESWIDGET_HPP

#include <QWidget>

namespace Ui {
class GlobalPropertiesWidget;
}

/**
 * @brief Widget for configuring global dashboard properties
 * 
 * This widget provides controls for dashboard-wide settings that
 * affect all plots or the overall dashboard behavior.
 */
class GlobalPropertiesWidget : public QWidget {
    Q_OBJECT

public:
    explicit GlobalPropertiesWidget(QWidget * parent = nullptr);
    ~GlobalPropertiesWidget() override;

signals:
    /**
     * @brief Emitted when global properties are changed
     */
    void globalPropertiesChanged();

private slots:
    /**
     * @brief Handle background color change
     */
    void handleBackgroundColorChanged();

    /**
     * @brief Handle grid visibility change
     */
    void handleGridVisibilityChanged(bool visible);

    /**
     * @brief Handle snap to grid change
     */
    void handleSnapToGridChanged(bool enabled);

private:
    Ui::GlobalPropertiesWidget * ui;
};

#endif// GLOBALPROPERTIESWIDGET_HPP