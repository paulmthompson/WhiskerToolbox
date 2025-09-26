#ifndef WHISKER_TRACING_WIDGET_HPP
#define WHISKER_TRACING_WIDGET_HPP

#include "DataTransform_Widget/TransformParameter_Widget/DataManagerParameter_Widget.hpp"

#include <QWidget>
#include <memory>
#include <string>

namespace Ui {
class WhiskerTracing_Widget;
}

/**
 * @brief Widget for configuring whisker tracing parameters
 */
class WhiskerTracing_Widget : public DataManagerParameter_Widget {
    Q_OBJECT

public:
    /**
     * @brief Constructor
     * @param parent Parent widget
     */
    explicit WhiskerTracing_Widget(QWidget * parent = nullptr);

    /**
     * @brief Destructor
     */
    ~WhiskerTracing_Widget() override;

    /**
     * @brief Gets the parameters configured in this widget
     * @return Unique pointer to the parameters
     */
    [[nodiscard]] std::unique_ptr<TransformParametersBase> getParameters() const override;

protected:
    void onDataManagerChanged() override;
    void onDataManagerDataChanged() override;

private slots:
    /**
     * @brief Slot called when use processed data checkbox changes
     * @param checked Whether to use processed data
     */
    void _onUseProcessedDataChanged(bool checked);

    /**
     * @brief Slot called when clip length spinbox value changes
     * @param value New clip length value
     */
    void _onClipLengthChanged(int value);

    /**
     * @brief Slot called when whisker length threshold spinbox value changes
     * @param value New whisker length threshold value
     */
    void _onWhiskerLengthThresholdChanged(double value);

    /**
     * @brief Slot called when batch size spinbox value changes
     * @param value New batch size value
     */
    void _onBatchSizeChanged(int value);

    /**
     * @brief Slot called when use parallel processing checkbox changes
     * @param checked Whether to use parallel processing
     */
    void _onUseParallelProcessingChanged(bool checked);

    /**
     * @brief Slot called when use mask data checkbox changes
     * @param checked Whether to use mask data
     */
    void _onUseMaskDataChanged(bool checked);

    /**
     * @brief Slot called when mask data combobox selection changes
     * @param index Selected index
     */
    void _onMaskDataChanged(int index);

private:
    Ui::WhiskerTracing_Widget * ui;

    std::string _selected_mask_key;

    void _refreshMaskKeys();
    void _updateMaskComboBox();
};

#endif// WHISKER_TRACING_WIDGET_HPP
