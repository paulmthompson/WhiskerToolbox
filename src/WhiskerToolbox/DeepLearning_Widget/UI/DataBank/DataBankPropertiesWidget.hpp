#ifndef DATA_BANK_PROPERTIES_WIDGET_HPP
#define DATA_BANK_PROPERTIES_WIDGET_HPP

/**
 * @file DataBankPropertiesWidget.hpp
 * @brief Properties panel section for capturing and managing DataBank entries.
 */

#include "models_v2/TensorSlotDescriptor.hpp"

#include <QWidget>

#include <memory>
#include <string>
#include <vector>

class DataManager;
class SlotAssembler;

namespace Ui {
class DataBankPropertiesWidget;
}

namespace dl::widget {

class DataSourceComboHelper;

/**
 * @brief Properties panel widget for capturing frames into the session DataBank.
 *
 * Provides target slot selection, optional memory index, DataManager source
 * picker, and a capture button. Entry IDs default to `{slot_name}_{memory_index}`.
 *
 * @note Not thread-safe — GUI thread only.
 */
class DataBankPropertiesWidget : public QWidget {
    Q_OBJECT

public:
    explicit DataBankPropertiesWidget(QWidget * parent = nullptr);
    ~DataBankPropertiesWidget() override;

    /**
     * @brief Connect to the session SlotAssembler used for capture.
     * @param assembler Non-owning pointer; must outlive this widget.
     */
    void setAssembler(SlotAssembler * assembler);

    /**
     * @brief Bind the DataManager used to populate the data-source combo.
     */
    void setDataManager(std::shared_ptr<DataManager> dm);

    /**
     * @brief Update the list of static input slots available for capture.
     */
    void setStaticSlots(
            std::vector<dl::TensorSlotDescriptor> const & static_slot_list);

    /**
     * @brief Enable or disable capture based on model weights readiness.
     */
    void setModelReady(bool ready);

    /**
     * @brief Set the frame index used when capturing.
     */
    void setCurrentFrame(int frame);

    /**
     * @brief Re-populate the data-source combo from DataManager.
     */
    void refreshDataSources();

    /**
     * @brief Refresh capture status for the currently selected entry ID.
     */
    void refreshCaptureStatus();

signals:
    /**
     * @brief Emitted after a successful capture or when bank contents change.
     */
    void dataBankChanged();

private slots:
    void _onSlotSelectionChanged();
    void _onMemoryIndexChanged();
    void _onCaptureClicked();

private:
    [[nodiscard]] dl::TensorSlotDescriptor const * _selectedSlot() const;
    [[nodiscard]] int _memoryIndex() const;
    [[nodiscard]] std::string _resolvedEntryId() const;

    void _updateMemoryIndexVisibility();
    void _updateEntryIdLabel();
    void _refreshDataSourceCombo();
    void _updateCaptureEnabled();
    void _setCaptureStatus(
            bool captured,
            int captured_frame,
            std::pair<float, float> value_range);

    std::unique_ptr<Ui::DataBankPropertiesWidget> _ui;
    SlotAssembler * _assembler = nullptr;
    std::shared_ptr<DataManager> _dm;
    std::unique_ptr<DataSourceComboHelper> _combo_helper;
    std::vector<dl::TensorSlotDescriptor> _static_slots;
    bool _model_ready = false;
    int _current_frame = 0;
};

}// namespace dl::widget

#endif// DATA_BANK_PROPERTIES_WIDGET_HPP
