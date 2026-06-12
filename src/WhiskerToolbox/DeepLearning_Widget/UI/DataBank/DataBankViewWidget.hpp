#ifndef DATA_BANK_VIEW_WIDGET_HPP
#define DATA_BANK_VIEW_WIDGET_HPP

/**
 * @file DataBankViewWidget.hpp
 * @brief View panel for displaying captured memory bank tensor previews.
 */

#include <QWidget>

#include <memory>

class DeepLearningState;
class SlotAssembler;

namespace Ui {
class DataBankViewWidget;
}

namespace dl::widget {

/**
 * @brief Displays metadata for captured static tensors in the memory bank.
 *
 * Shows shape, value range, provenance (data key and frame), and cache
 * status for each encoded entry resolved through SlotAssembler.
 */
class DataBankViewWidget : public QWidget {
    Q_OBJECT

public:
    explicit DataBankViewWidget(
            std::shared_ptr<DeepLearningState> state,
            QWidget * parent = nullptr);
    ~DataBankViewWidget() override;

    /**
     * @brief Set the SlotAssembler used to query cached tensors.
     *
     * @param assembler Non-owning pointer; must outlive this widget.
     */
    void setAssembler(SlotAssembler * assembler);

public slots:
    /**
     * @brief Refresh the captured tensor preview display.
     */
    void refresh();

private:
    void _clearPreviewArea();

    std::shared_ptr<DeepLearningState> _state;
    SlotAssembler * _assembler = nullptr;
    std::unique_ptr<Ui::DataBankViewWidget> _ui;
};

}// namespace dl::widget

#endif// DATA_BANK_VIEW_WIDGET_HPP
