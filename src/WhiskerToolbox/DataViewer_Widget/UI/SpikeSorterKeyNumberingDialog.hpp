/**
 * @file SpikeSorterKeyNumberingDialog.hpp
 * @brief Dialog for selecting series key numbering when loading a SpikeSorter config.
 */

#ifndef SPIKE_SORTER_KEY_NUMBERING_DIALOG_HPP
#define SPIKE_SORTER_KEY_NUMBERING_DIALOG_HPP

#include <QComboBox>
#include <QDialog>
#include <QDialogButtonBox>
#include <QLabel>
#include <QVBoxLayout>

/**
 * @brief Key numbering mode for mapping series suffixes to electrode channels.
 */
enum class SpikeSorterKeyNumberingMode {
    AutoDetect = 0,
    OneBased = 1,
    ZeroBased = 2
};

/**
 * @brief Minimal dialog for SpikeSorter series key numbering selection.
 */
class SpikeSorterKeyNumberingDialog : public QDialog {
    Q_OBJECT
public:
    /**
     * @brief Construct the dialog.
     * @param group_name Group whose keys are being ordered
     * @param auto_detected_one_based Auto-detected default (true = 1-based suffixes)
     * @param parent Optional Qt parent widget
     */
    explicit SpikeSorterKeyNumberingDialog(
            QString const & group_name,
            bool auto_detected_one_based,
            QWidget * parent = nullptr);

    /**
     * @brief Return the selected key numbering mode.
     */
    [[nodiscard]] SpikeSorterKeyNumberingMode numberingMode() const;

    /**
     * @brief Resolve whether series key suffixes should be treated as 1-based.
     * @param auto_detected_one_based Value from detectKeyOneBased when mode is AutoDetect
     */
    [[nodiscard]] bool keyOneBased(bool auto_detected_one_based) const;

private:
    QComboBox * _numbering_combo{nullptr};
};

#endif// SPIKE_SORTER_KEY_NUMBERING_DIALOG_HPP
