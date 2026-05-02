/**
 * @file SpikeToAnalogConfigDialog.cpp
 * @brief Dialog for configuring the spike-to-analog pairing loader.
 */

#include "SpikeToAnalogConfigDialog.hpp"

#include <QCheckBox>
#include <QFileDialog>
#include <QFormLayout>
#include <QGroupBox>
#include <QPushButton>

SpikeToAnalogConfigDialog::SpikeToAnalogConfigDialog(QWidget * parent)
    : QDialog(parent) {
    setWindowTitle(QStringLiteral("Load Spike-to-Analog Pairing"));
    setMinimumWidth(440);

    auto * layout = new QVBoxLayout(this);

    // ── CSV Format ──────────────────────────────────────────────────────────
    auto * format_box = new QGroupBox(QStringLiteral("CSV Format"), this);
    auto * format_form = new QFormLayout(format_box);

    // Delimiter
    _delimiter_combo = new QComboBox(this);
    _delimiter_combo->addItem(QStringLiteral("Comma  ( , )"),
                              static_cast<int>(SpikeToAnalogCsvDelimiter::Comma));
    _delimiter_combo->addItem(QStringLiteral("Tab  ( \\t )"),
                              static_cast<int>(SpikeToAnalogCsvDelimiter::Tab));
    _delimiter_combo->addItem(QStringLiteral("Semicolon  ( ; )"),
                              static_cast<int>(SpikeToAnalogCsvDelimiter::Semicolon));
    _delimiter_combo->addItem(QStringLiteral("Pipe  ( | )"),
                              static_cast<int>(SpikeToAnalogCsvDelimiter::Pipe));
    _delimiter_combo->addItem(QStringLiteral("Whitespace  (space / tab)"),
                              static_cast<int>(SpikeToAnalogCsvDelimiter::Whitespace));
    _delimiter_combo->setToolTip(QStringLiteral("Field separator used in the CSV file."));
    format_form->addRow(QStringLiteral("Delimiter:"), _delimiter_combo);

    // Digital channel column
    _digital_col_spin = new QSpinBox(this);
    _digital_col_spin->setMinimum(1);
    _digital_col_spin->setMaximum(99);
    _digital_col_spin->setValue(2);
    _digital_col_spin->setToolTip(QStringLiteral(
            "Column number (1 = first column) that contains the digital/spike channel index."));
    format_form->addRow(QStringLiteral("Digital channel column:"), _digital_col_spin);

    // Analog channel column
    _analog_col_spin = new QSpinBox(this);
    _analog_col_spin->setMinimum(1);
    _analog_col_spin->setMaximum(99);
    _analog_col_spin->setValue(3);
    _analog_col_spin->setToolTip(QStringLiteral(
            "Column number (1 = first column) that contains the analog/LFP channel index."));
    format_form->addRow(QStringLiteral("Analog channel column:"), _analog_col_spin);

    layout->addWidget(format_box);

    // ── Channel Indexing ────────────────────────────────────────────────────
    auto * index_box = new QGroupBox(QStringLiteral("Channel Indexing"), this);
    auto * index_form = new QFormLayout(index_box);

    auto makeIndexingCombo = [this]() {
        auto * combo = new QComboBox(this);
        combo->addItem(QStringLiteral("1-based  (first channel = 1)"), 1);
        combo->addItem(QStringLiteral("0-based  (first channel = 0)"), 0);
        combo->setToolTip(QStringLiteral(
                "Whether channel numbers in the CSV start at 1 or 0.\n"
                "1-based: channel 1 in file → series _0 in dataset.\n"
                "0-based: channel 0 in file → series _0 in dataset."));
        return combo;
    };

    _digital_indexing_combo = makeIndexingCombo();
    index_form->addRow(QStringLiteral("Digital channel indexing (CSV):"), _digital_indexing_combo);

    _analog_indexing_combo = makeIndexingCombo();
    index_form->addRow(QStringLiteral("Analog channel indexing (CSV):"), _analog_indexing_combo);

    // Key naming: how the destination series in the data manager are named
    auto makeKeyIndexingCombo = [this]() {
        auto * combo = new QComboBox(this);
        combo->addItem(QStringLiteral("1-based  (first series = '_1')"), 1);
        combo->addItem(QStringLiteral("0-based  (first series = '_0')"), 0);
        combo->setToolTip(QStringLiteral(
                "Whether the series names in the data manager use 1-based or 0-based numeric suffixes.\n"
                "1-based: the first channel is named e.g. 'spikes_1' or 'voltage_1'.\n"
                "0-based: the first channel is named e.g. 'spikes_0' or 'voltage_0'."));
        return combo;
    };

    _digital_key_indexing_combo = makeKeyIndexingCombo();
    index_form->addRow(QStringLiteral("Digital series key naming:"), _digital_key_indexing_combo);

    _analog_key_indexing_combo = makeKeyIndexingCombo();
    index_form->addRow(QStringLiteral("Analog series key naming:"), _analog_key_indexing_combo);

    layout->addWidget(index_box);

    // ── Series Prefixes & Placement ─────────────────────────────────────────
    auto * series_form = new QFormLayout;

    _digital_prefix_edit = new QLineEdit(QStringLiteral("spikes_"), this);
    _digital_prefix_edit->setToolTip(QStringLiteral(
            "Prefix used to identify digital (spike) series. "
            "Channel N (0-based) is looked up as <prefix>N."));
    series_form->addRow(QStringLiteral("Digital group prefix:"), _digital_prefix_edit);

    _analog_prefix_edit = new QLineEdit(QStringLiteral("voltage_"), this);
    _analog_prefix_edit->setToolTip(QStringLiteral(
            "Prefix used to identify analog (LFP) series. "
            "Channel M (0-based) is looked up as <prefix>M."));
    series_form->addRow(QStringLiteral("Analog group prefix:"), _analog_prefix_edit);

    _placement_combo = new QComboBox(this);
    _placement_combo->addItem(QStringLiteral("Adjacent Below"),
                              static_cast<int>(SpikeToAnalogPlacementMode::AdjacentBelow));
    _placement_combo->addItem(QStringLiteral("Adjacent Above"),
                              static_cast<int>(SpikeToAnalogPlacementMode::AdjacentAbove));
    _placement_combo->addItem(QStringLiteral("Overlay"),
                              static_cast<int>(SpikeToAnalogPlacementMode::Overlay));
    _placement_combo->setToolTip(QStringLiteral(
            "Adjacent Below / Above: spike series gets its own lane next to the analog lane.\n"
            "Overlay: spike series shares the analog series' lane band."));
    series_form->addRow(QStringLiteral("Placement mode:"), _placement_combo);

    _overlay_box_glyph_check = new QCheckBox(
            QStringLiteral("Render overlaid events as box markers"), this);
    _overlay_box_glyph_check->setToolTip(QStringLiteral(
            "When Placement is Overlay, draw each spike train as a box (time width from event options) "
            "instead of tick marks. Other placement modes ignore this option."));
    _overlay_box_glyph_check->setEnabled(false);
    series_form->addRow(_overlay_box_glyph_check);

    connect(_placement_combo, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &SpikeToAnalogConfigDialog::_syncOverlayBoxGlyphControl);

    layout->addLayout(series_form);

    // ── File path ───────────────────────────────────────────────────────────
    auto * file_row_layout = new QHBoxLayout;
    _file_path_edit = new QLineEdit(this);
    _file_path_edit->setPlaceholderText(QStringLiteral("Select CSV pairing file…"));
    _file_path_edit->setReadOnly(true);
    _browse_button = new QPushButton(QStringLiteral("Browse…"), this);
    file_row_layout->addWidget(_file_path_edit);
    file_row_layout->addWidget(_browse_button);

    auto * file_form = new QFormLayout;
    file_form->addRow(new QLabel(QStringLiteral("CSV pairing file:"), this), file_row_layout);
    layout->addLayout(file_form);

    // ── Buttons ─────────────────────────────────────────────────────────────
    _button_box = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
    layout->addWidget(_button_box);

    // OK enabled only when a file has been selected
    _button_box->button(QDialogButtonBox::Ok)->setEnabled(false);
    connect(_file_path_edit, &QLineEdit::textChanged, this, [this](QString const & text) {
        _button_box->button(QDialogButtonBox::Ok)->setEnabled(!text.trimmed().isEmpty());
    });

    connect(_browse_button, &QPushButton::clicked, this, &SpikeToAnalogConfigDialog::_onBrowseClicked);
    connect(_button_box, &QDialogButtonBox::accepted, this, &QDialog::accept);
    connect(_button_box, &QDialogButtonBox::rejected, this, &QDialog::reject);

    _syncOverlayBoxGlyphControl();
}

void SpikeToAnalogConfigDialog::_syncOverlayBoxGlyphControl() {
    bool const overlay = (placementMode() == SpikeToAnalogPlacementMode::Overlay);
    _overlay_box_glyph_check->setEnabled(overlay);
    if (!overlay) {
        _overlay_box_glyph_check->setChecked(false);
    }
}

void SpikeToAnalogConfigDialog::_onBrowseClicked() {
    QString const path = QFileDialog::getOpenFileName(
            this,
            QStringLiteral("Open Spike-to-Analog Pairing CSV"),
            QString(),
            QStringLiteral("CSV files (*.csv *.txt);;All files (*)"));
    if (!path.isEmpty()) {
        _file_path_edit->setText(path);
    }
}

SpikeToAnalogCsvDelimiter SpikeToAnalogConfigDialog::delimiter() const {
    return static_cast<SpikeToAnalogCsvDelimiter>(_delimiter_combo->currentData().toInt());
}

int SpikeToAnalogConfigDialog::digitalColumn() const {
    return _digital_col_spin->value();
}

int SpikeToAnalogConfigDialog::analogColumn() const {
    return _analog_col_spin->value();
}

bool SpikeToAnalogConfigDialog::digitalOneBased() const {
    return _digital_indexing_combo->currentData().toInt() == 1;
}

bool SpikeToAnalogConfigDialog::analogOneBased() const {
    return _analog_indexing_combo->currentData().toInt() == 1;
}

bool SpikeToAnalogConfigDialog::digitalKeyOneBased() const {
    return _digital_key_indexing_combo->currentData().toInt() == 1;
}

bool SpikeToAnalogConfigDialog::analogKeyOneBased() const {
    return _analog_key_indexing_combo->currentData().toInt() == 1;
}

std::string SpikeToAnalogConfigDialog::digitalGroupPrefix() const {
    return _digital_prefix_edit->text().toStdString();
}

std::string SpikeToAnalogConfigDialog::analogGroupPrefix() const {
    return _analog_prefix_edit->text().toStdString();
}

SpikeToAnalogPlacementMode SpikeToAnalogConfigDialog::placementMode() const {
    return static_cast<SpikeToAnalogPlacementMode>(
            _placement_combo->currentData().toInt());
}

std::string SpikeToAnalogConfigDialog::filePath() const {
    return _file_path_edit->text().toStdString();
}

bool SpikeToAnalogConfigDialog::useBoxGlyph() const {
    return _overlay_box_glyph_check->isChecked();
}
