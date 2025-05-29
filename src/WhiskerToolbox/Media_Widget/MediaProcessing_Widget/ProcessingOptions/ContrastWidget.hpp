#ifndef CONTRAST_WIDGET_HPP
#define CONTRAST_WIDGET_HPP

#include "DataManager/utils/ProcessingOptions.hpp"

#include <QWidget>

namespace Ui {
class ContrastWidget;
}

/**
 * @brief Widget for controlling linear contrast/brightness transformation options
 * 
 * This widget provides UI controls for adjusting contrast (alpha) and brightness (beta)
 * parameters. It emits signals when options change but does not apply any processing itself.
 */
class ContrastWidget : public QWidget {
    Q_OBJECT

public:
    explicit ContrastWidget(QWidget* parent = nullptr);
    ~ContrastWidget() override;

    /**
     * @brief Get the current contrast options
     * @return Current ContrastOptions structure
     */
    ContrastOptions getOptions() const;

    /**
     * @brief Set the contrast options and update UI controls
     * @param options ContrastOptions to apply to the widget
     */
    void setOptions(ContrastOptions const& options);

signals:
    /**
     * @brief Emitted when any contrast option changes
     * @param options Updated ContrastOptions structure
     */
    void optionsChanged(ContrastOptions const& options);

private slots:
    void _onActiveChanged();
    void _onAlphaChanged();
    void _onBetaChanged();

private:
    Ui::ContrastWidget* ui;

    void _updateOptions();
    void _blockSignalsAndSetValues(ContrastOptions const& options);
};

#endif // CONTRAST_WIDGET_HPP 