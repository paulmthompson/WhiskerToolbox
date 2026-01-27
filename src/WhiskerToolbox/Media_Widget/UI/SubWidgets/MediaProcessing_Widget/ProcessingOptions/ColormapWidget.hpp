#ifndef COLORMAP_WIDGET_HPP
#define COLORMAP_WIDGET_HPP

#include "ImageProcessing/ProcessingOptions.hpp"

#include <QWidget>

namespace Ui {
class ColormapWidget;
}

/**
 * @brief Widget for controlling colormap options for grayscale images
 * 
 * This widget provides UI controls for selecting colormap type and adjusting
 * alpha blending parameters. It emits signals when options change but does not 
 * apply any processing itself.
 */
class ColormapWidget : public QWidget {
    Q_OBJECT

public:
    explicit ColormapWidget(QWidget* parent = nullptr);
    ~ColormapWidget() override;

    /**
     * @brief Get the current colormap options
     * @return Current ColormapOptions structure
     */
    ColormapOptions getOptions() const;

    /**
     * @brief Set the colormap options and update UI controls
     * @param options ColormapOptions to apply to the widget
     */
    void setOptions(ColormapOptions const& options);

    /**
     * @brief Set whether the widget should be enabled (for grayscale images only)
     * @param enabled True if the media is grayscale and colormap can be applied
     */
    void setColormapEnabled(bool enabled);

signals:
    /**
     * @brief Emitted when any colormap option changes
     * @param options Updated ColormapOptions structure
     */
    void optionsChanged(ColormapOptions const& options);

private slots:
    void _onActiveChanged();
    void _onColormapTypeChanged();
    void _onAlphaChanged();
    void _onNormalizeChanged();

private:
    Ui::ColormapWidget* ui;

    void _updateOptions();
    void _blockSignalsAndSetValues(ColormapOptions const& options);
    void _populateColormapComboBox();
    ColormapType _getColormapTypeFromIndex(int index) const;
    int _getIndexFromColormapType(ColormapType type) const;
};

#endif // COLORMAP_WIDGET_HPP
