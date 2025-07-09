#include "DraggableListWidget.hpp"

#include <QDrag>
#include <QMimeData>
#include <QApplication>
#include <QListWidgetItem>
#include <QPainter>
#include <QPixmap>

DraggableListWidget::DraggableListWidget(QWidget* parent)
    : QListWidget(parent) {
    
    setDragDropMode(QAbstractItemView::DragOnly);
    setDefaultDropAction(Qt::CopyAction);
}

void DraggableListWidget::startDrag(Qt::DropActions supportedActions) {
    QListWidgetItem* item = currentItem();
    if (!item) {
        return;
    }

    QMimeData* mime_data = mimeData({item});
    if (!mime_data) {
        return;
    }

    QDrag* drag = new QDrag(this);
    drag->setMimeData(mime_data);
    
    // Set drag pixmap to the item's icon or text
    if (!item->icon().isNull()) {
        drag->setPixmap(item->icon().pixmap(32, 32));
    } else {
        // Create a simple text pixmap if no icon
        QPixmap pixmap(100, 30);
        pixmap.fill(Qt::lightGray);
        QPainter painter(&pixmap);
        painter.setPen(Qt::black);
        painter.drawText(pixmap.rect(), Qt::AlignCenter, item->text());
        drag->setPixmap(pixmap);
    }

    drag->exec(supportedActions);
}

QMimeData* DraggableListWidget::mimeData(const QList<QListWidgetItem*> items) const {
    if (items.isEmpty()) {
        return nullptr;
    }

    QMimeData* mime_data = new QMimeData();
    
    // Get the plot type from the item's data
    QListWidgetItem* item = items.first();
    QString plot_type = item->data(Qt::UserRole).toString();
    
    if (!plot_type.isEmpty()) {
        mime_data->setData("application/x-plottype", plot_type.toUtf8());
        mime_data->setText(item->text()); // Also set text for fallback
    }

    return mime_data;
}

QStringList DraggableListWidget::mimeTypes() const {
    return {"application/x-plottype", "text/plain"};
} 