/**
 * @file DataSynthesizerView_Widget.hpp
 * @brief Preview/view panel for the DataSynthesizer widget
 *
 * Embeds a SynthesizerPreviewWidget (QOpenGLWidget) that renders a
 * generated AnalogTimeSeries as a polyline for visual verification
 * before committing data to DataManager.
 */

#ifndef DATA_SYNTHESIZER_VIEW_WIDGET_HPP
#define DATA_SYNTHESIZER_VIEW_WIDGET_HPP

#include <QWidget>

#include <memory>

class AnalogTimeSeries;
class DataManager;
class DataSynthesizerState;
class SynthesizerPreviewWidget;

/**
 * @brief View/preview panel for the Data Synthesizer
 *
 * Hosts a SynthesizerPreviewWidget for OpenGL signal preview.
 */
class DataSynthesizerView_Widget : public QWidget {
    Q_OBJECT

public:
    explicit DataSynthesizerView_Widget(
            std::shared_ptr<DataSynthesizerState> state,
            std::shared_ptr<DataManager> data_manager,
            QWidget * parent = nullptr);

    ~DataSynthesizerView_Widget() override = default;

public slots:
    /**
     * @brief Set preview data from a generated AnalogTimeSeries
     *
     * Called when the user clicks "Preview" in the properties panel.
     *
     * @param series The generated time series to preview
     */
    void setPreviewData(std::shared_ptr<AnalogTimeSeries> series);

private:
    std::shared_ptr<DataSynthesizerState> _state;
    std::shared_ptr<DataManager> _data_manager;
    SynthesizerPreviewWidget * _preview_widget = nullptr;
};

#endif// DATA_SYNTHESIZER_VIEW_WIDGET_HPP
