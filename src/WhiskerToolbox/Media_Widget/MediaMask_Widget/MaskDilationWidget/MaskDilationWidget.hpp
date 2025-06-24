#ifndef MASK_DILATION_WIDGET_HPP
#define MASK_DILATION_WIDGET_HPP

#include "DataManager/utils/ProcessingOptions.hpp"

#include <QWidget>

namespace Ui {
class MaskDilationWidget;
}

/**
 * @brief Widget for controlling mask dilation and erosion options
 * 
 * This widget provides UI controls for growing or shrinking masks using morphological operations.
 * It supports both preview mode and permanent application of changes.
 */
class MaskDilationWidget : public QWidget {
    Q_OBJECT

public:
    explicit MaskDilationWidget(QWidget * parent = nullptr);
    ~MaskDilationWidget() override;

    /**
     * @brief Get the current mask dilation options
     * @return Current MaskDilationOptions structure
     */
    MaskDilationOptions getOptions() const;

    /**
     * @brief Set the mask dilation options and update UI controls
     * @param options MaskDilationOptions to apply to the widget
     */
    void setOptions(MaskDilationOptions const & options);

signals:
    /**
     * @brief Emitted when any dilation option changes
     * @param options Updated MaskDilationOptions structure
     */
    void optionsChanged(MaskDilationOptions const & options);

    /**
     * @brief Emitted when user clicks the apply button
     */
    void applyRequested();

private slots:
    void _onPreviewChanged();
    void _onSizeChanged();
    void _onModeChanged();
    void _onApplyClicked();

private:
    Ui::MaskDilationWidget * ui;

    void _updateOptions();
    void _blockSignalsAndSetValues(MaskDilationOptions const & options);
};

#endif// MASK_DILATION_WIDGET_HPP