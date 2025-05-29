#ifndef GAMMA_WIDGET_HPP
#define GAMMA_WIDGET_HPP

#include "DataManager/utils/ProcessingOptions.hpp"

#include <QWidget>

namespace Ui {
class GammaWidget;
}

/**
 * @brief Widget for controlling gamma correction options
 * 
 * This widget provides UI controls for adjusting gamma correction values.
 * It emits signals when options change but does not apply any processing itself.
 */
class GammaWidget : public QWidget {
    Q_OBJECT

public:
    explicit GammaWidget(QWidget* parent = nullptr);
    ~GammaWidget() override;

    /**
     * @brief Get the current gamma options
     * @return Current GammaOptions structure
     */
    GammaOptions getOptions() const;

    /**
     * @brief Set the gamma options and update UI controls
     * @param options GammaOptions to apply to the widget
     */
    void setOptions(GammaOptions const& options);

signals:
    /**
     * @brief Emitted when any gamma option changes
     * @param options Updated GammaOptions structure
     */
    void optionsChanged(GammaOptions const& options);

private slots:
    void _onActiveChanged();
    void _onGammaChanged();

private:
    Ui::GammaWidget* ui;

    void _updateOptions();
    void _blockSignalsAndSetValues(GammaOptions const& options);
};

#endif // GAMMA_WIDGET_HPP 