/**
 * @file DataSynthesizerProperties_Widget.hpp
 * @brief Properties panel for the DataSynthesizer widget
 *
 * Milestone 2a placeholder — displays a label. Will be populated with
 * generator selection combos and AutoParamWidget in Milestone 2b.
 */

#ifndef DATA_SYNTHESIZER_PROPERTIES_WIDGET_HPP
#define DATA_SYNTHESIZER_PROPERTIES_WIDGET_HPP

#include <QWidget>

#include <memory>

class DataSynthesizerState;

/**
 * @brief Properties panel for the Data Synthesizer
 *
 * Currently a placeholder widget displaying a label.
 * Will house output type combo, generator combo, AutoParamWidget,
 * output key field, and Generate button in Milestone 2b.
 */
class DataSynthesizerProperties_Widget : public QWidget {
    Q_OBJECT

public:
    explicit DataSynthesizerProperties_Widget(
            std::shared_ptr<DataSynthesizerState> state,
            QWidget * parent = nullptr);

    ~DataSynthesizerProperties_Widget() override = default;

private:
    std::shared_ptr<DataSynthesizerState> _state;
};

#endif// DATA_SYNTHESIZER_PROPERTIES_WIDGET_HPP
