/**
 * @file DataSynthesizerView_Widget.hpp
 * @brief Preview/view panel for the DataSynthesizer widget
 *
 * Milestone 2a placeholder — displays a label. Will be replaced with
 * an OpenGL preview widget (SynthesizerPreviewWidget) in Milestone 2c.
 */

#ifndef DATA_SYNTHESIZER_VIEW_WIDGET_HPP
#define DATA_SYNTHESIZER_VIEW_WIDGET_HPP

#include <QWidget>

#include <memory>

class DataManager;
class DataSynthesizerState;

/**
 * @brief View/preview panel for the Data Synthesizer
 *
 * Currently a placeholder widget displaying a label.
 * Will embed a SynthesizerPreviewWidget for OpenGL signal preview
 * in Milestone 2c.
 */
class DataSynthesizerView_Widget : public QWidget {
    Q_OBJECT

public:
    explicit DataSynthesizerView_Widget(
            std::shared_ptr<DataSynthesizerState> state,
            std::shared_ptr<DataManager> data_manager,
            QWidget * parent = nullptr);

    ~DataSynthesizerView_Widget() override = default;

private:
    std::shared_ptr<DataSynthesizerState> _state;
    std::shared_ptr<DataManager> _data_manager;
};

#endif// DATA_SYNTHESIZER_VIEW_WIDGET_HPP
