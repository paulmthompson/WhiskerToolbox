#ifndef TRANSFORMS_V2_PROPERTIES_WIDGET_HPP
#define TRANSFORMS_V2_PROPERTIES_WIDGET_HPP

/**
 * @file TransformsV2Properties_Widget.hpp
 * @brief Properties panel widget for TransformsV2 transform pipelines
 *
 * This widget provides the UI for configuring and executing TransformsV2
 * transform pipelines. It is placed in the Right (properties) zone.
 */

#include <QWidget>

#include <memory>

class TransformsV2State;

namespace Ui {
class TransformsV2Properties_Widget;
}

class TransformsV2Properties_Widget : public QWidget {
    Q_OBJECT

public:
    explicit TransformsV2Properties_Widget(std::shared_ptr<TransformsV2State> state,
                                           QWidget * parent = nullptr);
    ~TransformsV2Properties_Widget() override;

private:
    std::unique_ptr<Ui::TransformsV2Properties_Widget> ui;
    std::shared_ptr<TransformsV2State> _state;
};

#endif // TRANSFORMS_V2_PROPERTIES_WIDGET_HPP
