#include "ScatterPlotPropertiesWidget.hpp"

#include "Core/ScatterAxisSource.hpp"
#include "Core/ScatterPlotState.hpp"
#include "Core/SourceCompatibility.hpp"
#include "DataManager/DataManager.hpp"
#include "DataManager/AnalogTimeSeries/Analog_Time_Series.hpp"
#include "DataManager/Tensors/TensorData.hpp"
#include "Plots/Common/HorizontalAxisWidget/HorizontalAxisWithRangeControls.hpp"
#include "Plots/Common/VerticalAxisWidget/VerticalAxisWithRangeControls.hpp"
#include "Collapsible_Widget/Section.hpp"
#include "Plots/Common/GlyphStyleWidget/GlyphStyleControls.hpp"
#include "UI/ScatterPlotWidget.hpp"

#include <QCheckBox>
#include <QComboBox>
#include <QFormLayout>
#include <QLabel>
#include <QSpinBox>
#include <QVBoxLayout>

#include "ui_ScatterPlotPropertiesWidget.h"

namespace {

/// Prefix used to identify AnalogTimeSeries keys in the combo box
constexpr char const * ATS_PREFIX = "[ATS] ";
/// Prefix used to identify TensorData keys in the combo box
constexpr char const * TD_PREFIX = "[Tensor] ";

/// Format a data key with its type prefix for display in combo boxes
QString formatKeyItem(std::string const & key, char const * prefix)
{
    return QString::fromStdString(prefix + key);
}

/// Extract the raw data key from a combo box display string
std::string extractKey(QString const & display_text)
{
    auto text = display_text.toStdString();
    // Strip known prefixes
    for (auto const * prefix : {ATS_PREFIX, TD_PREFIX}) {
        auto len = std::string_view(prefix).size();
        if (text.size() > len && text.substr(0, len) == prefix) {
            return text.substr(len);
        }
    }
    return text;
}

}  // namespace

ScatterPlotPropertiesWidget::ScatterPlotPropertiesWidget(std::shared_ptr<ScatterPlotState> state,
                                                         std::shared_ptr<DataManager> data_manager,
                                                         QWidget * parent)
    : QWidget(parent),
      ui(new Ui::ScatterPlotPropertiesWidget),
      _state(std::move(state)),
      _data_manager(std::move(data_manager))
{
    ui->setupUi(this);

    _createDataSourceUI();

    if (_data_manager) {
        _dm_observer_id = _data_manager->addObserver([this]() {
            _populateKeyComboBoxes();
        });
    }

    if (_state) {
        _updateUIFromState();
    }
}

ScatterPlotPropertiesWidget::~ScatterPlotPropertiesWidget()
{
    if (_data_manager && _dm_observer_id != -1) {
        _data_manager->removeObserver(_dm_observer_id);
    }
    delete ui;
}

void ScatterPlotPropertiesWidget::_createDataSourceUI()
{
    // === Data Sources Section ===
    _data_source_section = new Section(this, "Data Sources");

    auto * form = new QFormLayout();
    form->setContentsMargins(4, 4, 4, 4);
    form->setSpacing(4);

    // --- X Axis ---
    auto * x_header = new QLabel("<b>X Axis</b>");
    form->addRow(x_header);

    _x_key_combo = new QComboBox();
    _x_key_combo->setPlaceholderText("Select data source...");
    form->addRow("Key:", _x_key_combo);

    _x_column_combo = new QComboBox();
    _x_column_combo->setPlaceholderText("Column");
    _x_column_combo->setVisible(false);
    form->addRow("Column:", _x_column_combo);

    _x_offset_spin = new QSpinBox();
    _x_offset_spin->setRange(-10000, 10000);
    _x_offset_spin->setValue(0);
    _x_offset_spin->setToolTip("Temporal offset in TimeFrameIndex steps");
    form->addRow("Offset:", _x_offset_spin);

    // --- Y Axis ---
    auto * y_header = new QLabel("<b>Y Axis</b>");
    form->addRow(y_header);

    _y_key_combo = new QComboBox();
    _y_key_combo->setPlaceholderText("Select data source...");
    form->addRow("Key:", _y_key_combo);

    _y_column_combo = new QComboBox();
    _y_column_combo->setPlaceholderText("Column");
    _y_column_combo->setVisible(false);
    form->addRow("Column:", _y_column_combo);

    _y_offset_spin = new QSpinBox();
    _y_offset_spin->setRange(-10000, 10000);
    _y_offset_spin->setValue(0);
    _y_offset_spin->setToolTip("Temporal offset in TimeFrameIndex steps");
    form->addRow("Offset:", _y_offset_spin);

    // --- Compatibility status ---
    _compatibility_label = new QLabel();
    _compatibility_label->setWordWrap(true);
    form->addRow(_compatibility_label);

    auto * content_widget = new QWidget();
    content_widget->setLayout(form);
    _data_source_section->setContentLayout(*form);

    ui->main_layout->insertWidget(0, _data_source_section);

    // === Reference Line Section ===
    _reference_line_section = new Section(this, "Reference Line");

    auto * ref_layout = new QVBoxLayout();
    ref_layout->setContentsMargins(4, 4, 4, 4);

    _reference_line_checkbox = new QCheckBox("Show y = x reference line");
    ref_layout->addWidget(_reference_line_checkbox);

    _reference_line_section->setContentLayout(*ref_layout);

    // Insert after data source section
    int insert_idx = ui->main_layout->indexOf(_data_source_section) + 1;
    ui->main_layout->insertWidget(insert_idx, _reference_line_section);

    // === Glyph Style Section ===
    _glyph_style_section = new Section(this, "Glyph Options");

    if (_state) {
        _glyph_style_controls = new GlyphStyleControls(_state->glyphStyleState(), this);

        auto * glyph_layout = new QVBoxLayout();
        glyph_layout->setContentsMargins(4, 4, 4, 4);
        glyph_layout->addWidget(_glyph_style_controls);
        _glyph_style_section->setContentLayout(*glyph_layout);
    }

    // Insert after reference line section
    int glyph_insert_idx = ui->main_layout->indexOf(_reference_line_section) + 1;
    ui->main_layout->insertWidget(glyph_insert_idx, _glyph_style_section);

    // === Selection Mode Section ===
    _selection_section = new Section(this, "Selection");

    auto * sel_layout = new QVBoxLayout();
    sel_layout->setContentsMargins(4, 4, 4, 4);
    sel_layout->setSpacing(4);

    auto * sel_form = new QFormLayout();
    sel_form->setContentsMargins(0, 0, 0, 0);
    _selection_mode_combo = new QComboBox();
    _selection_mode_combo->addItem("Single Point");
    _selection_mode_combo->addItem("Polygon");
    sel_form->addRow("Mode:", _selection_mode_combo);
    sel_layout->addLayout(sel_form);

    _selection_instructions_label = new QLabel();
    _selection_instructions_label->setWordWrap(true);
    _selection_instructions_label->setStyleSheet("color: #888; font-size: 11px;");
    sel_layout->addWidget(_selection_instructions_label);

    _selection_section->setContentLayout(*sel_layout);

    int sel_insert_idx = ui->main_layout->indexOf(_glyph_style_section) + 1;
    ui->main_layout->insertWidget(sel_insert_idx, _selection_section);

    _updateSelectionInstructions();

    // --- Connections ---
    connect(_x_key_combo, &QComboBox::currentIndexChanged,
            this, [this]() { if (!_updating_combos) _onXKeyChanged(); });
    connect(_y_key_combo, &QComboBox::currentIndexChanged,
            this, [this]() { if (!_updating_combos) _onYKeyChanged(); });
    connect(_x_column_combo, &QComboBox::currentIndexChanged,
            this, [this]() { if (!_updating_combos) _onXColumnChanged(); });
    connect(_y_column_combo, &QComboBox::currentIndexChanged,
            this, [this]() { if (!_updating_combos) _onYColumnChanged(); });
    connect(_x_offset_spin, &QSpinBox::valueChanged,
            this, &ScatterPlotPropertiesWidget::_onXOffsetChanged);
    connect(_y_offset_spin, &QSpinBox::valueChanged,
            this, &ScatterPlotPropertiesWidget::_onYOffsetChanged);
    connect(_reference_line_checkbox, &QCheckBox::toggled,
            this, &ScatterPlotPropertiesWidget::_onReferenceLineToggled);
    connect(_selection_mode_combo, &QComboBox::currentIndexChanged,
            this, &ScatterPlotPropertiesWidget::_onSelectionModeChanged);

    // Populate initial data
    _populateKeyComboBoxes();
}

void ScatterPlotPropertiesWidget::_populateKeyComboBoxes()
{
    _updating_combos = true;

    // Save current selections
    auto const x_current = _x_key_combo->currentText();
    auto const y_current = _y_key_combo->currentText();

    _x_key_combo->clear();
    _y_key_combo->clear();

    // Add empty "none" option
    _x_key_combo->addItem("");
    _y_key_combo->addItem("");

    if (!_data_manager) {
        _updating_combos = false;
        return;
    }

    // Add AnalogTimeSeries keys
    auto ats_keys = _data_manager->getKeys<AnalogTimeSeries>();
    for (auto const & key : ats_keys) {
        auto display = formatKeyItem(key, ATS_PREFIX);
        _x_key_combo->addItem(display);
        _y_key_combo->addItem(display);
    }

    // Add TensorData keys
    auto td_keys = _data_manager->getKeys<TensorData>();
    for (auto const & key : td_keys) {
        auto display = formatKeyItem(key, TD_PREFIX);
        _x_key_combo->addItem(display);
        _y_key_combo->addItem(display);
    }

    // Restore previous selections if still valid, otherwise handle stale keys
    int x_idx = _x_key_combo->findText(x_current);
    if (x_idx >= 0) {
        _x_key_combo->setCurrentIndex(x_idx);
    } else if (!x_current.isEmpty()) {
        // Key was deleted from DataManager — clear the X source
        _x_column_combo->clear();
        _x_column_combo->setVisible(false);
        if (_state) {
            _state->setXSource(std::nullopt);
        }
    }

    int y_idx = _y_key_combo->findText(y_current);
    if (y_idx >= 0) {
        _y_key_combo->setCurrentIndex(y_idx);
    } else if (!y_current.isEmpty()) {
        // Key was deleted from DataManager — clear the Y source
        _y_column_combo->clear();
        _y_column_combo->setVisible(false);
        if (_state) {
            _state->setYSource(std::nullopt);
        }
    }

    _updating_combos = false;

    // Refresh column combos for current selections
    if (_x_key_combo->currentIndex() > 0) {
        _populateColumnComboBox(_x_column_combo, extractKey(_x_key_combo->currentText()));
    }
    if (_y_key_combo->currentIndex() > 0) {
        _populateColumnComboBox(_y_column_combo, extractKey(_y_key_combo->currentText()));
    }

    _updateCompatibilityLabel();
}

void ScatterPlotPropertiesWidget::_populateColumnComboBox(QComboBox * combo, std::string const & data_key)
{
    if (!_data_manager || data_key.empty()) {
        combo->clear();
        combo->setVisible(false);
        return;
    }

    auto tensor = _data_manager->getData<TensorData>(data_key);
    if (!tensor) {
        combo->clear();
        combo->setVisible(false);
        return;
    }

    // Show the column combo for TensorData
    combo->setVisible(true);

    auto const prev_text = combo->currentText();
    combo->clear();

    auto const & col_names = tensor->columnNames();
    if (!col_names.empty()) {
        for (auto const & name : col_names) {
            combo->addItem(QString::fromStdString(name));
        }
    } else {
        // Fallback to numeric indices
        auto const num_cols = tensor->numColumns();
        for (std::size_t i = 0; i < num_cols; ++i) {
            combo->addItem(QString::number(static_cast<int>(i)));
        }
    }

    // Restore previous selection
    int idx = combo->findText(prev_text);
    if (idx >= 0) {
        combo->setCurrentIndex(idx);
    } else if (combo->count() > 0) {
        combo->setCurrentIndex(0);
    }
}

void ScatterPlotPropertiesWidget::_onXKeyChanged()
{
    auto key_text = _x_key_combo->currentText();
    if (key_text.isEmpty()) {
        _x_column_combo->clear();
        _x_column_combo->setVisible(false);
        if (_state) {
            _state->setXSource(std::nullopt);
        }
        _updateCompatibilityLabel();
        return;
    }

    auto key = extractKey(key_text);
    _populateColumnComboBox(_x_column_combo, key);
    _applyXSourceToState();
    _updateCompatibilityLabel();
}

void ScatterPlotPropertiesWidget::_onYKeyChanged()
{
    auto key_text = _y_key_combo->currentText();
    if (key_text.isEmpty()) {
        _y_column_combo->clear();
        _y_column_combo->setVisible(false);
        if (_state) {
            _state->setYSource(std::nullopt);
        }
        _updateCompatibilityLabel();
        return;
    }

    auto key = extractKey(key_text);
    _populateColumnComboBox(_y_column_combo, key);
    _applyYSourceToState();
    _updateCompatibilityLabel();
}

void ScatterPlotPropertiesWidget::_onXColumnChanged()
{
    _applyXSourceToState();
}

void ScatterPlotPropertiesWidget::_onYColumnChanged()
{
    _applyYSourceToState();
}

void ScatterPlotPropertiesWidget::_onXOffsetChanged(int /*value*/)
{
    _applyXSourceToState();
}

void ScatterPlotPropertiesWidget::_onYOffsetChanged(int /*value*/)
{
    _applyYSourceToState();
}

void ScatterPlotPropertiesWidget::_onReferenceLineToggled(bool checked)
{
    if (_state) {
        _state->setShowReferenceLine(checked);
    }
}

void ScatterPlotPropertiesWidget::_onSelectionModeChanged(int index)
{
    if (_updating_combos || !_state) {
        return;
    }
    auto mode = (index == 1) ? ScatterSelectionMode::Polygon : ScatterSelectionMode::SinglePoint;
    _state->setSelectionMode(mode);
    _updateSelectionInstructions();
}

void ScatterPlotPropertiesWidget::_updateSelectionInstructions()
{
    if (!_selection_instructions_label || !_selection_mode_combo) {
        return;
    }

    int const idx = _selection_mode_combo->currentIndex();
    if (idx == 0) {
        // Single Point
        _selection_instructions_label->setText(
            "Ctrl+Click: toggle point selection\n"
            "Shift+Click: remove from selection\n"
            "Right-click: group context menu");
    } else {
        // Polygon
        _selection_instructions_label->setText(
            "Ctrl+Click: add polygon vertex\n"
            "Enter: close polygon & select enclosed points\n"
            "Backspace: undo last vertex\n"
            "Escape: cancel polygon\n"
            "Right-click: group context menu");
    }
}

void ScatterPlotPropertiesWidget::_applyXSourceToState()
{
    if (!_state) {
        return;
    }

    auto key_text = _x_key_combo->currentText();
    if (key_text.isEmpty()) {
        _state->setXSource(std::nullopt);
        return;
    }

    ScatterAxisSource source;
    source.data_key = extractKey(key_text);
    source.time_offset = _x_offset_spin->value();

    // Set column info if this is a TensorData source with a column selected
    if (_x_column_combo->isVisible() && _x_column_combo->currentIndex() >= 0) {
        auto col_text = _x_column_combo->currentText().toStdString();
        // Try to parse as integer index
        bool is_numeric = false;
        auto col_idx = static_cast<std::size_t>(_x_column_combo->currentText().toInt(&is_numeric));
        if (is_numeric && _data_manager) {
            auto tensor = _data_manager->getData<TensorData>(source.data_key);
            if (tensor && tensor->columnNames().empty()) {
                source.tensor_column_index = col_idx;
            } else {
                source.tensor_column_name = col_text;
            }
        } else {
            source.tensor_column_name = col_text;
        }
    }

    _state->setXSource(source);
}

void ScatterPlotPropertiesWidget::_applyYSourceToState()
{
    if (!_state) {
        return;
    }

    auto key_text = _y_key_combo->currentText();
    if (key_text.isEmpty()) {
        _state->setYSource(std::nullopt);
        return;
    }

    ScatterAxisSource source;
    source.data_key = extractKey(key_text);
    source.time_offset = _y_offset_spin->value();

    if (_y_column_combo->isVisible() && _y_column_combo->currentIndex() >= 0) {
        auto col_text = _y_column_combo->currentText().toStdString();
        bool is_numeric = false;
        auto col_idx = static_cast<std::size_t>(_y_column_combo->currentText().toInt(&is_numeric));
        if (is_numeric && _data_manager) {
            auto tensor = _data_manager->getData<TensorData>(source.data_key);
            if (tensor && tensor->columnNames().empty()) {
                source.tensor_column_index = col_idx;
            } else {
                source.tensor_column_name = col_text;
            }
        } else {
            source.tensor_column_name = col_text;
        }
    }

    _state->setYSource(source);
}

void ScatterPlotPropertiesWidget::_updateCompatibilityLabel()
{
    if (!_compatibility_label || !_state || !_data_manager) {
        return;
    }

    auto const & x_src = _state->xSource();
    auto const & y_src = _state->ySource();

    if (!x_src.has_value() || !y_src.has_value()) {
        _compatibility_label->setText("");
        _compatibility_label->setStyleSheet("");
        return;
    }

    auto result = checkSourceCompatibility(*_data_manager, *x_src, *y_src);
    if (result.compatible) {
        _compatibility_label->setText("Sources compatible");
        _compatibility_label->setStyleSheet("color: green;");
    } else {
        _compatibility_label->setText(QString::fromStdString(result.warning_message));
        _compatibility_label->setStyleSheet("color: red;");
    }
}

void ScatterPlotPropertiesWidget::setPlotWidget(ScatterPlotWidget * plot_widget)
{
    _plot_widget = plot_widget;
    if (!_plot_widget || !_state) {
        return;
    }

    auto * horizontal_axis_state = _state->horizontalAxisState();
    if (horizontal_axis_state) {
        _horizontal_range_controls_section = new Section(this, "X-Axis Range Controls");
        _horizontal_range_controls = new HorizontalAxisRangeControls(horizontal_axis_state, _horizontal_range_controls_section);
        _horizontal_range_controls_section->autoSetContentLayout();
        // Insert after reference line section
        int insert_index = _reference_line_section
                ? ui->main_layout->indexOf(_reference_line_section) + 1
                : ui->main_layout->indexOf(_data_source_section) + 1;
        ui->main_layout->insertWidget(insert_index, _horizontal_range_controls_section);
    }

    auto * vertical_axis_state = _state->verticalAxisState();
    if (vertical_axis_state) {
        _vertical_range_controls_section = new Section(this, "Y-Axis Range Controls");
        _vertical_range_controls = new VerticalAxisRangeControls(vertical_axis_state, _vertical_range_controls_section);
        _vertical_range_controls_section->autoSetContentLayout();
        int insert_index = _horizontal_range_controls_section
                ? ui->main_layout->indexOf(_horizontal_range_controls_section) + 1
                : (_reference_line_section
                    ? ui->main_layout->indexOf(_reference_line_section) + 1
                    : 0);
        ui->main_layout->insertWidget(insert_index, _vertical_range_controls_section);
    }
}

void ScatterPlotPropertiesWidget::_updateUIFromState()
{
    if (!_state) {
        return;
    }

    _updating_combos = true;

    // Restore X source
    auto const & x_src = _state->xSource();
    if (x_src.has_value() && !x_src->data_key.empty()) {
        // Determine the type prefix
        if (_data_manager) {
            auto ats = _data_manager->getData<AnalogTimeSeries>(x_src->data_key);
            auto display = ats
                ? formatKeyItem(x_src->data_key, ATS_PREFIX)
                : formatKeyItem(x_src->data_key, TD_PREFIX);
            int idx = _x_key_combo->findText(display);
            if (idx >= 0) {
                _x_key_combo->setCurrentIndex(idx);
                _populateColumnComboBox(_x_column_combo, x_src->data_key);
                // Set the column
                if (x_src->tensor_column_name.has_value()) {
                    int col_idx = _x_column_combo->findText(
                        QString::fromStdString(*x_src->tensor_column_name));
                    if (col_idx >= 0) {
                        _x_column_combo->setCurrentIndex(col_idx);
                    }
                } else if (x_src->tensor_column_index.has_value()) {
                    _x_column_combo->setCurrentIndex(
                        static_cast<int>(*x_src->tensor_column_index));
                }
            }
        }
        _x_offset_spin->setValue(x_src->time_offset);
    }

    // Restore Y source
    auto const & y_src = _state->ySource();
    if (y_src.has_value() && !y_src->data_key.empty()) {
        if (_data_manager) {
            auto ats = _data_manager->getData<AnalogTimeSeries>(y_src->data_key);
            auto display = ats
                ? formatKeyItem(y_src->data_key, ATS_PREFIX)
                : formatKeyItem(y_src->data_key, TD_PREFIX);
            int idx = _y_key_combo->findText(display);
            if (idx >= 0) {
                _y_key_combo->setCurrentIndex(idx);
                _populateColumnComboBox(_y_column_combo, y_src->data_key);
                if (y_src->tensor_column_name.has_value()) {
                    int col_idx = _y_column_combo->findText(
                        QString::fromStdString(*y_src->tensor_column_name));
                    if (col_idx >= 0) {
                        _y_column_combo->setCurrentIndex(col_idx);
                    }
                } else if (y_src->tensor_column_index.has_value()) {
                    _y_column_combo->setCurrentIndex(
                        static_cast<int>(*y_src->tensor_column_index));
                }
            }
        }
        _y_offset_spin->setValue(y_src->time_offset);
    }

    // Reference line
    if (_reference_line_checkbox) {
        _reference_line_checkbox->setChecked(_state->showReferenceLine());
    }

    // Selection mode
    if (_selection_mode_combo) {
        auto mode = _state->selectionMode();
        _selection_mode_combo->setCurrentIndex(
            mode == ScatterSelectionMode::Polygon ? 1 : 0);
        _updateSelectionInstructions();
    }

    _updating_combos = false;

    _updateCompatibilityLabel();
}
