/**
 * @file SeriesOrderingDialog.cpp
 * @brief Dialog for placing one series above or below another in the DataViewer lane stack.
 */

#include "SeriesOrderingDialog.hpp"

#include <QPushButton>

SeriesOrderingDialog::SeriesOrderingDialog(std::string const & source_key,
                                           std::vector<std::string> const & available_keys,
                                           QWidget * parent)
    : QDialog(parent) {
    setWindowTitle(QStringLiteral("Place Relative To…"));
    setMinimumWidth(380);

    // Build string list for the completer
    QStringList key_list;
    key_list.reserve(static_cast<int>(available_keys.size()));
    for (auto const & k: available_keys) {
        key_list.append(QString::fromStdString(k));
    }

    auto * layout = new QVBoxLayout(this);

    // Context label
    auto * context_label = new QLabel(
            QStringLiteral("Repositioning: <b>%1</b>").arg(QString::fromStdString(source_key)),
            this);
    layout->addWidget(context_label);

    // Form
    auto * form = new QFormLayout;

    _placement_combo = new QComboBox(this);
    _placement_combo->addItem(QStringLiteral("Above"));
    _placement_combo->addItem(QStringLiteral("Below"));
    form->addRow(QStringLiteral("Placement:"), _placement_combo);

    _target_edit = new QLineEdit(this);
    _target_edit->setPlaceholderText(QStringLiteral("Type a series key…"));

    auto * completer = new QCompleter(key_list, this);
    completer->setCaseSensitivity(Qt::CaseInsensitive);
    completer->setFilterMode(Qt::MatchContains);
    completer->setCompletionMode(QCompleter::PopupCompletion);
    _target_edit->setCompleter(completer);

    form->addRow(QStringLiteral("Target series:"), _target_edit);
    layout->addLayout(form);

    // Buttons
    _button_box = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
    layout->addWidget(_button_box);

    // OK enabled only when the field is non-empty
    _button_box->button(QDialogButtonBox::Ok)->setEnabled(false);
    connect(_target_edit, &QLineEdit::textChanged, this, [this](QString const & text) {
        _button_box->button(QDialogButtonBox::Ok)->setEnabled(!text.trimmed().isEmpty());
    });

    connect(_button_box, &QDialogButtonBox::accepted, this, &QDialog::accept);
    connect(_button_box, &QDialogButtonBox::rejected, this, &QDialog::reject);
}

std::string SeriesOrderingDialog::targetKey() const {
    return _target_edit->text().trimmed().toStdString();
}

bool SeriesOrderingDialog::placeAbove() const {
    return _placement_combo->currentIndex() == 0;
}
