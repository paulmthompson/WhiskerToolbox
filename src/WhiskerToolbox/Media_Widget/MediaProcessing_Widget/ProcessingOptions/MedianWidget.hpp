#ifndef MEDIAN_WIDGET_HPP
#define MEDIAN_WIDGET_HPP

#include "ImageProcessing/ProcessingOptions.hpp"

#include <QWidget>

namespace Ui {
class MedianWidget;
}

/**
 * @brief Widget for controlling median filtering options
 * 
 * This widget provides UI controls for adjusting median filter parameters.
 * It emits signals when options change but does not apply any processing itself.
 */
class MedianWidget : public QWidget {
    Q_OBJECT

public:
    explicit MedianWidget(QWidget* parent = nullptr);
    ~MedianWidget() override;

    /**
     * @brief Get the current median filter options
     * @return Current MedianOptions structure
     */
    MedianOptions getOptions() const;

    /**
     * @brief Set the median filter options and update UI controls
     * @param options MedianOptions to apply to the widget
     */
    void setOptions(MedianOptions const& options);

signals:
    /**
     * @brief Emitted when any median filter option changes
     * @param options Updated MedianOptions structure
     */
    void optionsChanged(MedianOptions const& options);

private slots:
    void _onActiveChanged();
    void _onKernelSizeChanged();

private:
    Ui::MedianWidget* ui;

    void _updateOptions();
    void _blockSignalsAndSetValues(MedianOptions const& options);
};

#endif // MEDIAN_WIDGET_HPP 