#ifndef DRAGGABLELISTWIDGET_HPP
#define DRAGGABLELISTWIDGET_HPP

#include <QListWidget>

class QDrag;
class QMimeData;

/**
 * @brief Custom QListWidget that supports dragging plot types
 * 
 * This widget allows users to drag plot type items from the toolbox
 * to the dashboard graphics view to create new plots.
 */
class DraggableListWidget : public QListWidget {
    Q_OBJECT

public:
    explicit DraggableListWidget(QWidget* parent = nullptr);

protected:
    /**
     * @brief Start drag operation when user drags an item
     */
    void startDrag(Qt::DropActions supportedActions) override;

    /**
     * @brief Create mime data for the dragged item
     * @param items List of items being dragged
     * @return Mime data containing plot type information
     */
    QMimeData* mimeData(const QList<QListWidgetItem*> items) const;

    /**
     * @brief Get supported mime types for drag operations
     * @return List of supported mime types
     */
    QStringList mimeTypes() const override;
};

#endif // DRAGGABLELISTWIDGET_HPP 