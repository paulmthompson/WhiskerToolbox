#ifndef SHARPEN_WIDGET_HPP
#define SHARPEN_WIDGET_HPP

#include "DataManager/utils/ProcessingOptions.hpp"

#include <QWidget>

namespace Ui {
class SharpenWidget;
}

/**
 * @brief Widget for controlling image sharpening options
 * 
 * This widget provides UI controls for adjusting image sharpening parameters.
 * It emits signals when options change but does not apply any processing itself.
 */
class SharpenWidget : public QWidget {
    Q_OBJECT

public:
    explicit SharpenWidget(QWidget* parent = nullptr);
    ~SharpenWidget() override;

    /**
     * @brief Get the current sharpen options
     * @return Current SharpenOptions structure
     */
    SharpenOptions getOptions() const;

    /**
     * @brief Set the sharpen options and update UI controls
     * @param options SharpenOptions to apply to the widget
     */
    void setOptions(SharpenOptions const& options);

signals:
    /**
     * @brief Emitted when any sharpen option changes
     * @param options Updated SharpenOptions structure
     */
    void optionsChanged(SharpenOptions const& options);

private slots:
    void _onActiveChanged();
    void _onSigmaChanged();

private:
    Ui::SharpenWidget* ui;

    void _updateOptions();
    void _blockSignalsAndSetValues(SharpenOptions const& options);
};

#endif // SHARPEN_WIDGET_HPP 