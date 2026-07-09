/**
 * @file SpikeSorterKeyNumberingDialog.cpp
 * @brief Dialog for selecting series key numbering when loading a SpikeSorter config.
 */

#include "SpikeSorterKeyNumberingDialog.hpp"

#include <QFormLayout>
#include <QGroupBox>

SpikeSorterKeyNumberingDialog::SpikeSorterKeyNumberingDialog(
        QString const & group_name,
        bool auto_detected_one_based,
        QWidget * parent)
    : QDialog(parent) {
    setWindowTitle(QStringLiteral("SpikeSorter Key Numbering"));
    setMinimumWidth(420);

    auto * layout = new QVBoxLayout(this);

    auto * intro = new QLabel(
            QStringLiteral("Select how series key suffixes map to electrode channels for group \"%1\".")
                    .arg(group_name),
            this);
    intro->setWordWrap(true);
    layout->addWidget(intro);

    auto * group_box = new QGroupBox(QStringLiteral("Series Key Numbering"), this);
    auto * form = new QFormLayout(group_box);

    _numbering_combo = new QComboBox(this);
    _numbering_combo->addItem(
            QStringLiteral("Auto-detect  (%1)")
                    .arg(auto_detected_one_based
                                 ? QStringLiteral("detected 1-based, e.g. voltage_1")
                                 : QStringLiteral("detected 0-based, e.g. voltage_0")),
            static_cast<int>(SpikeSorterKeyNumberingMode::AutoDetect));
    _numbering_combo->addItem(
            QStringLiteral("1-based  (first series = '_1')"),
            static_cast<int>(SpikeSorterKeyNumberingMode::OneBased));
    _numbering_combo->addItem(
            QStringLiteral("0-based  (first series = '_0')"),
            static_cast<int>(SpikeSorterKeyNumberingMode::ZeroBased));
    _numbering_combo->setToolTip(QStringLiteral(
            "SpikeSorter electrode files use 1-based channel numbers.\n"
            "This setting controls how series key suffixes map to those channels.\n"
            "Auto-detect treats suffix 0 as 0-based naming."));
    form->addRow(QStringLiteral("Key numbering:"), _numbering_combo);
    layout->addWidget(group_box);

    auto * buttons = new QDialogButtonBox(
            QDialogButtonBox::Ok | QDialogButtonBox::Cancel,
            this);
    connect(buttons, &QDialogButtonBox::accepted, this, &QDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
    layout->addWidget(buttons);
}

SpikeSorterKeyNumberingMode SpikeSorterKeyNumberingDialog::numberingMode() const {
    return static_cast<SpikeSorterKeyNumberingMode>(_numbering_combo->currentData().toInt());
}

bool SpikeSorterKeyNumberingDialog::keyOneBased(bool auto_detected_one_based) const {
    switch (numberingMode()) {
        case SpikeSorterKeyNumberingMode::OneBased:
            return true;
        case SpikeSorterKeyNumberingMode::ZeroBased:
            return false;
        case SpikeSorterKeyNumberingMode::AutoDetect:
        default:
            return auto_detected_one_based;
    }
}
