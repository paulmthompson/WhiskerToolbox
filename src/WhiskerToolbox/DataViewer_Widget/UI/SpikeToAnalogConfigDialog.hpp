/**
 * @file SpikeToAnalogConfigDialog.hpp
 * @brief Dialog for configuring the spike-to-analog pairing loader.
 *
 * Provides fields for the field delimiter, column positions for the digital and
 * analog channel numbers, index-base policy (1-based vs 0-based), digital and
 * analog series group prefixes, placement mode, and the CSV file path.
 */

#ifndef SPIKE_TO_ANALOG_CONFIG_DIALOG_HPP
#define SPIKE_TO_ANALOG_CONFIG_DIALOG_HPP

#include "Ordering/SpikeToAnalogPairingLoader.hpp"

#include <QComboBox>
#include <QDialog>
#include <QDialogButtonBox>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QSpinBox>
#include <QVBoxLayout>

#include <string>

/**
 * @brief Configuration dialog for the spike-to-analog pairing loader.
 *
 * After exec(), use the accessor methods to retrieve the user's selections.
 */
class SpikeToAnalogConfigDialog : public QDialog {
    Q_OBJECT
public:
    /**
     * @brief Construct the dialog.
     *
     * @param parent Optional Qt parent widget.
     */
    explicit SpikeToAnalogConfigDialog(QWidget * parent = nullptr);

    /** @brief Return the field delimiter selected by the user. */
    [[nodiscard]] SpikeToAnalogCsvDelimiter delimiter() const;

    /** @brief Return the 1-based column index for the digital channel number. */
    [[nodiscard]] int digitalColumn() const;

    /** @brief Return the 1-based column index for the analog channel number. */
    [[nodiscard]] int analogColumn() const;

    /** @brief Return true if digital channel numbers in the CSV are 1-based. */
    [[nodiscard]] bool digitalOneBased() const;

    /** @brief Return true if analog channel numbers in the CSV are 1-based. */
    [[nodiscard]] bool analogOneBased() const;

    /** @brief Return true if digital series key suffixes in the data manager are 1-based
     *  (e.g. "spikes_1" is the first spike channel). */
    [[nodiscard]] bool digitalKeyOneBased() const;

    /** @brief Return true if analog series key suffixes in the data manager are 1-based
     *  (e.g. "voltage_1" is the first analog channel). */
    [[nodiscard]] bool analogKeyOneBased() const;

    /**
     * @brief Return the digital group prefix entered by the user (e.g. "spikes_").
     */
    [[nodiscard]] std::string digitalGroupPrefix() const;

    /**
     * @brief Return the analog group prefix entered by the user (e.g. "voltage_").
     */
    [[nodiscard]] std::string analogGroupPrefix() const;

    /**
     * @brief Return the placement mode selected by the user.
     */
    [[nodiscard]] SpikeToAnalogPlacementMode placementMode() const;

    /**
     * @brief Return the absolute path to the CSV file chosen by the user.
     *
     * @return Empty string if no file was selected.
     */
    [[nodiscard]] std::string filePath() const;

private slots:
    void _onBrowseClicked();

private:
    QComboBox * _delimiter_combo{nullptr};
    QSpinBox * _digital_col_spin{nullptr};
    QSpinBox * _analog_col_spin{nullptr};
    QComboBox * _digital_indexing_combo{nullptr};
    QComboBox * _analog_indexing_combo{nullptr};
    QComboBox * _digital_key_indexing_combo{nullptr};
    QComboBox * _analog_key_indexing_combo{nullptr};
    QLineEdit * _digital_prefix_edit{nullptr};
    QLineEdit * _analog_prefix_edit{nullptr};
    QComboBox * _placement_combo{nullptr};
    QLineEdit * _file_path_edit{nullptr};
    QPushButton * _browse_button{nullptr};
    QDialogButtonBox * _button_box{nullptr};
};

#endif// SPIKE_TO_ANALOG_CONFIG_DIALOG_HPP
