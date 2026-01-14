#ifndef PLOTDOCKWIDGETCONTENT_HPP
#define PLOTDOCKWIDGETCONTENT_HPP

#include <QString>
#include <QWidget>

class QGraphicsScene;
class QGraphicsView;
class AbstractPlotWidget;

/**
 * @brief Dock content widget containing a per-plot scene and view
 */
class PlotDockWidgetContent : public QWidget {
    Q_OBJECT

public:
    /**
     * @brief Construct content for a plot dock
     * @param plot_id Identifier
     * @param plot_item The existing plot item (QGraphicsItem subclass)
     * @param parent QWidget parent
     */
    explicit PlotDockWidgetContent(QString const & plot_id,
                                   AbstractPlotWidget * plot_item,
                                   QWidget * parent = nullptr);
    ~PlotDockWidgetContent() override = default;

signals:
    /**
     * @brief Emitted when this content becomes active (focus/click)
     */
    void activated(QString const & plot_id);

protected:
    void focusInEvent(QFocusEvent * event) override;
    void resizeEvent(QResizeEvent * event) override;
    bool eventFilter(QObject * watched, QEvent * event) override;

private:
    QString _plot_id;
    QGraphicsScene * _scene;
    QGraphicsView * _view;
    AbstractPlotWidget * _plot_item{nullptr};

    void _initView(AbstractPlotWidget * plot_item);
    void _fitPlotToView();
};

#endif// PLOTDOCKWIDGETCONTENT_HPP
