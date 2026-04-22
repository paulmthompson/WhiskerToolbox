/**
 * @file SeriesOrderingDialog.hpp
 * @brief Dialog for placing one series above or below another in the DataViewer lane stack.
 *
 * Provides a QLineEdit with QCompleter (suitable for high-channel-count datasets
 * such as 384-channel Neuropixel probes) and a QComboBox to choose "Above" or "Below".
 */

#ifndef SERIES_ORDERING_DIALOG_HPP
#define SERIES_ORDERING_DIALOG_HPP

#include <QComboBox>
#include <QCompleter>
#include <QDialog>
#include <QDialogButtonBox>
#include <QFormLayout>
#include <QLabel>
#include <QLineEdit>
#include <QStringList>
#include <QVBoxLayout>

#include <string>
#include <vector>

/**
 * @brief Modal dialog for relative series placement.
 *
 * The user types a partial key in the QLineEdit; the QCompleter filters live
 * matches from the supplied @p available_keys list. After exec(), use
 * targetKey() and placeAbove() to retrieve the result.
 */
class SeriesOrderingDialog : public QDialog {
    Q_OBJECT
public:
    /**
     * @brief Construct the dialog.
     *
     * @param source_key      The series being repositioned (shown as context label).
     * @param available_keys  All other currently-displayed series keys offered as targets.
     * @param parent          Optional Qt parent widget.
     */
    explicit SeriesOrderingDialog(std::string const & source_key,
                                  std::vector<std::string> const & available_keys,
                                  QWidget * parent = nullptr);

    /**
     * @brief Return the key the user selected as the placement target.
     * @return Empty string if no valid selection was made.
     */
    [[nodiscard]] std::string targetKey() const;

    /**
     * @brief Return true if the source should be placed above the target.
     * @return true = above, false = below.
     */
    [[nodiscard]] bool placeAbove() const;

private:
    QLineEdit * _target_edit{nullptr};
    QComboBox * _placement_combo{nullptr};
    QDialogButtonBox * _button_box{nullptr};
};

#endif// SERIES_ORDERING_DIALOG_HPP
