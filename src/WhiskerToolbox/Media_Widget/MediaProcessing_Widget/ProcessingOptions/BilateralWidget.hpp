#ifndef BILATERAL_WIDGET_HPP
#define BILATERAL_WIDGET_HPP

#include "ImageProcessing/ProcessingOptions.hpp"

#include <QWidget>

namespace Ui {
class BilateralWidget;
}

/**
 * @brief Widget for controlling bilateral filter options
 * 
 * This widget provides UI controls for adjusting bilateral filter parameters.
 * It emits signals when options change but does not apply any processing itself.
 */
class BilateralWidget : public QWidget {
    Q_OBJECT

public:
    explicit BilateralWidget(QWidget* parent = nullptr);
    ~BilateralWidget() override;

    /**
     * @brief Get the current bilateral options
     * @return Current BilateralOptions structure
     */
    BilateralOptions getOptions() const;

    /**
     * @brief Set the bilateral options and update UI controls
     * @param options BilateralOptions to apply to the widget
     */
    void setOptions(BilateralOptions const& options);

signals:
    /**
     * @brief Emitted when any bilateral option changes
     * @param options Updated BilateralOptions structure
     */
    void optionsChanged(BilateralOptions const& options);

private slots:
    void _onActiveChanged();
    void _onDChanged();
    void _onSigmaColorChanged();
    void _onSigmaSpaceChanged();

private:
    Ui::BilateralWidget* ui;

    void _updateOptions();
    void _blockSignalsAndSetValues(BilateralOptions const& options);
};

#endif // BILATERAL_WIDGET_HPP 