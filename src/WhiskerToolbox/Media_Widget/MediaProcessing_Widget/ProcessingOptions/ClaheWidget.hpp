#ifndef CLAHE_WIDGET_HPP
#define CLAHE_WIDGET_HPP

#include "ImageProcessing/ProcessingOptions.hpp"

#include <QWidget>

namespace Ui {
class ClaheWidget;
}

/**
 * @brief Widget for controlling CLAHE (Contrast Limited Adaptive Histogram Equalization) options
 * 
 * This widget provides UI controls for adjusting CLAHE parameters.
 * It emits signals when options change but does not apply any processing itself.
 */
class ClaheWidget : public QWidget {
    Q_OBJECT

public:
    explicit ClaheWidget(QWidget* parent = nullptr);
    ~ClaheWidget() override;

    /**
     * @brief Get the current CLAHE options
     * @return Current ClaheOptions structure
     */
    ClaheOptions getOptions() const;

    /**
     * @brief Set the CLAHE options and update UI controls
     * @param options ClaheOptions to apply to the widget
     */
    void setOptions(ClaheOptions const& options);

signals:
    /**
     * @brief Emitted when any CLAHE option changes
     * @param options Updated ClaheOptions structure
     */
    void optionsChanged(ClaheOptions const& options);

private slots:
    void _onActiveChanged();
    void _onClipLimitChanged();
    void _onGridSizeChanged();

private:
    Ui::ClaheWidget* ui;

    void _updateOptions();
    void _blockSignalsAndSetValues(ClaheOptions const& options);
};

#endif // CLAHE_WIDGET_HPP 