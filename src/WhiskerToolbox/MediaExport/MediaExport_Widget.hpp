#ifndef MEDIA_EXPORT_WIDGET_HPP
#define MEDIA_EXPORT_WIDGET_HPP

#include <QWidget>
#include "media_export.hpp" // For MediaExportOptions struct

namespace Ui {
class MediaExport_Widget;
}

class MediaExport_Widget : public QWidget {
    Q_OBJECT

public:
    explicit MediaExport_Widget(QWidget *parent = nullptr);
    ~MediaExport_Widget() override;

    MediaExportOptions getOptions() const; // Public method to get current options

private slots:
    void _updatePrefixAndPaddingState(bool checked); // To enable/disable prefix and padding based on save_by_frame_name

private:
    Ui::MediaExport_Widget *ui;
};

#endif // MEDIA_EXPORT_WIDGET_HPP
