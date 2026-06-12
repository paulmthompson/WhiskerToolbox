#ifndef DATA_BANK_PROPERTIES_WIDGET_HPP
#define DATA_BANK_PROPERTIES_WIDGET_HPP

/**
 * @file DataBankPropertiesWidget.hpp
 * @brief Properties panel section for the deep learning memory bank.
 */

#include <QWidget>

#include <memory>

namespace Ui {
class DataBankPropertiesWidget;
}

namespace dl::widget {

/**
 * @brief Properties panel widget for configuring and inspecting DataBank entries.
 *
 * Phase 4 placeholder: displays the Memory Bank section header. Future work
 * will add entry list, capture controls, and bank_entry_id pickers.
 */
class DataBankPropertiesWidget : public QWidget {
    Q_OBJECT

public:
    explicit DataBankPropertiesWidget(QWidget * parent = nullptr);
    ~DataBankPropertiesWidget() override;

private:
    std::unique_ptr<Ui::DataBankPropertiesWidget> _ui;
};

}// namespace dl::widget

#endif// DATA_BANK_PROPERTIES_WIDGET_HPP
