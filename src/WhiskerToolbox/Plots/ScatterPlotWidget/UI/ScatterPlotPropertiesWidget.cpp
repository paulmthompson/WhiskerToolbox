#include "ScatterPlotPropertiesWidget.hpp"

#include "AnalogTimeSeries/Analog_Time_Series.hpp"
#include "Collapsible_Widget/Section.hpp"
#include "Core/ScatterAxisSource.hpp"
#include "Core/ScatterColorConfig.hpp"
#include "Core/ScatterPlotState.hpp"
#include "Core/SourceCompatibility.hpp"
#include "CorePlotting/FeatureColor/FeatureColorCompatibility.hpp"
#include "CorePlotting/FeatureColor/FeatureColorSourceDescriptor.hpp"
#include "CorePlotting/FeatureColor/PlotSourceRowType.hpp"
#include "DataManager/DataManager.hpp"
#include "DigitalTimeSeries/Digital_Interval_Series.hpp"
#include "Plots/Common/ColormapControls/ColormapControls.hpp"
#include "Plots/Common/GlyphStyleWidget/GlyphStyleControls.hpp"
#include "Plots/Common/HorizontalAxisWidget/HorizontalAxisWithRangeControls.hpp"
#include "Plots/Common/SelectionInstructions.hpp"
#include "Plots/Common/VerticalAxisWidget/VerticalAxisWithRangeControls.hpp"
#include "Tensors/TensorData.hpp"
#include "UI/ScatterPlotWidget.hpp"

#include <QCheckBox>
#include <QColorDialog>
#include <QComboBox>
#include <QDoubleSpinBox>
#include <QFormLayout>
#include <QLabel>
#include <QPushButton>
#include <QSpinBox>
#include <QVBoxLayout>

#include "ui_ScatterPlotPropertiesWidget.h"

namespace {

/// Prefix used to identify AnalogTimeSeries keys in the combo box
constexpr char const * ATS_PREFIX = "[ATS] ";
/// Prefix used to identify TensorData keys in the combo box
constexpr char const * TD_PREFIX = "[Tensor] ";
/// Prefix used to identify DigitalIntervalSeries keys in the combo box
constexpr char const * DIS_PREFIX = "[Interval] ";

/// Format a data key with its type prefix for display in combo boxes
QString formatKeyItem(std::string const & key, char const * prefix) {
    return QString::fromStdString(prefix + key);
}

/// Extract the raw data key from a combo box display string
std::string extractKey(QString const & display_text) {
    auto text = display_text.toStdString();
    // Strip known prefixes
    for (auto const * prefix: {ATS_PREFIX, TD_PREFIX, DIS_PREFIX}) {
        auto len = std::string_view(prefix).size();
        if (text.size() > len && text.substr(0, len) == prefix) {
            return text.substr(len);
        }
    }
    return text;
}

}// namespace

ScatterPlotPropertiesWidget::ScatterPlotPropertiesWidget(std::shared_ptr<ScatterPlotState> state,
                                                         std::shared_ptr<DataManager> data_manager,
                                                         QWidget * parent)
    : QWidget(parent),
      ui(new Ui::ScatterPlotPropertiesWidget),
      _state(std::move(state)),
      _data_manager(std::move(data_manager)) {
    ui->setupUi(this);

    _createDataSourceUI();
    _createPointColoringUI();

    if (_data_manager) {
        _dm_observer_id = _data_manager->addObserver([this]() {
            _populateKeyComboBoxes();
            _populateColorKeyCombo();
        });
    }

    if (_state) {
        _updateUIFromState();
    }
}

ScatterPlotPropertiesWidget::~ScatterPlotPropertiesWidget() {
    if (_data_manager && _dm_observer_id != -1) {
        _data_manager->removeObserver(_dm_observer_id);
    }
    delete ui;
}

void ScatterPlotPropertiesWidget::_createDataSourceUI() {
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
    int const insert_idx = ui->main_layout->indexOf(_data_source_section) + 1;
    ui->main_layout->insertWidget(insert_idx, _reference_line_section);

    // === Glyph Style Section ===
    _glyph_style_section = new Section(this, "Glyph Options");

    if (_state) {
        _glyph_style_controls = new GlyphStyleControls(_state->glyphStyleState(), this);

        auto * glyph_layout = new QVBoxLayout();
        glyph_layout->setContentsMargins(4, 4, 4, 4);
        glyph_layout->addWidget(_glyph_style_controls);

        // Color by group checkbox
        _color_by_group_checkbox = new QCheckBox("Color by group assignment");
        _color_by_group_checkbox->setToolTip(
                "When enabled, points are colored according to their group color.\n"
                "Points not in any group use the default glyph color above.");
        _color_by_group_checkbox->setChecked(_state->colorByGroup());
        glyph_layout->addWidget(_color_by_group_checkbox);

        // Show cluster labels checkbox
        _show_cluster_labels_checkbox = new QCheckBox("Show cluster labels");
        _show_cluster_labels_checkbox->setToolTip(
                "When enabled, labels are drawn at cluster centroids\n"
                "showing group name and point count.");
        _show_cluster_labels_checkbox->setChecked(_state->showClusterLabels());
        glyph_layout->addWidget(_show_cluster_labels_checkbox);

        _glyph_style_section->setContentLayout(*glyph_layout);
    }

    // Insert after reference line section
    int const glyph_insert_idx = ui->main_layout->indexOf(_reference_line_section) + 1;
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

    int const sel_insert_idx = ui->main_layout->indexOf(_glyph_style_section) + 1;
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
    if (_color_by_group_checkbox) {
        connect(_color_by_group_checkbox, &QCheckBox::toggled,
                this, &ScatterPlotPropertiesWidget::_onColorByGroupToggled);
    }
    if (_show_cluster_labels_checkbox) {
        connect(_show_cluster_labels_checkbox, &QCheckBox::toggled,
                this, [this](bool checked) {
                    if (_state) {
                        _state->setShowClusterLabels(checked);
                    }
                });
    }
    connect(_selection_mode_combo, &QComboBox::currentIndexChanged,
            this, &ScatterPlotPropertiesWidget::_onSelectionModeChanged);

    // Populate initial data
    _populateKeyComboBoxes();
}

void ScatterPlotPropertiesWidget::_populateKeyComboBoxes() {
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
    for (auto const & key: ats_keys) {
        auto display = formatKeyItem(key, ATS_PREFIX);
        _x_key_combo->addItem(display);
        _y_key_combo->addItem(display);
    }

    // Add TensorData keys
    auto td_keys = _data_manager->getKeys<TensorData>();
    for (auto const & key: td_keys) {
        auto display = formatKeyItem(key, TD_PREFIX);
        _x_key_combo->addItem(display);
        _y_key_combo->addItem(display);
    }

    // Restore previous selections if still valid, otherwise handle stale keys
    int const x_idx = _x_key_combo->findText(x_current);
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

    int const y_idx = _y_key_combo->findText(y_current);
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

void ScatterPlotPropertiesWidget::_populateColumnComboBox(QComboBox * combo, std::string const & data_key) {
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
        for (auto const & name: col_names) {
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
    int const idx = combo->findText(prev_text);
    if (idx >= 0) {
        combo->setCurrentIndex(idx);
    } else if (combo->count() > 0) {
        combo->setCurrentIndex(0);
    }
}

void ScatterPlotPropertiesWidget::_onXKeyChanged() {
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

void ScatterPlotPropertiesWidget::_onYKeyChanged() {
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

void ScatterPlotPropertiesWidget::_onXColumnChanged() {
    _applyXSourceToState();
}

void ScatterPlotPropertiesWidget::_onYColumnChanged() {
    _applyYSourceToState();
}

void ScatterPlotPropertiesWidget::_onXOffsetChanged(int /*value*/) {
    _applyXSourceToState();
}

void ScatterPlotPropertiesWidget::_onYOffsetChanged(int /*value*/) {
    _applyYSourceToState();
}

void ScatterPlotPropertiesWidget::_onReferenceLineToggled(bool checked) {
    if (_state) {
        _state->setShowReferenceLine(checked);
    }
}

void ScatterPlotPropertiesWidget::_onColorByGroupToggled(bool checked) {
    if (_state) {
        _state->setColorByGroup(checked);
    }
}

void ScatterPlotPropertiesWidget::_onSelectionModeChanged(int index) {
    if (_updating_combos || !_state) {
        return;
    }
    auto mode = (index == 1) ? ScatterSelectionMode::Polygon : ScatterSelectionMode::SinglePoint;
    _state->setSelectionMode(mode);
    _updateSelectionInstructions();
}

void ScatterPlotPropertiesWidget::_updateSelectionInstructions() {
    if (!_selection_instructions_label || !_selection_mode_combo) {
        return;
    }

    int const idx = _selection_mode_combo->currentIndex();
    if (idx == 0) {
        // Single Point
        _selection_instructions_label->setText(
                WhiskerToolbox::Plots::SelectionInstructions::singlePoint());
    } else {
        // Polygon
        _selection_instructions_label->setText(
                WhiskerToolbox::Plots::SelectionInstructions::polygon());
    }
}

void ScatterPlotPropertiesWidget::_applyXSourceToState() {
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

void ScatterPlotPropertiesWidget::_applyYSourceToState() {
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

void ScatterPlotPropertiesWidget::_updateCompatibilityLabel() {
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

void ScatterPlotPropertiesWidget::setPlotWidget(ScatterPlotWidget * plot_widget) {
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
        int const insert_index = _reference_line_section
                                         ? ui->main_layout->indexOf(_reference_line_section) + 1
                                         : ui->main_layout->indexOf(_data_source_section) + 1;
        ui->main_layout->insertWidget(insert_index, _horizontal_range_controls_section);
    }

    auto * vertical_axis_state = _state->verticalAxisState();
    if (vertical_axis_state) {
        _vertical_range_controls_section = new Section(this, "Y-Axis Range Controls");
        _vertical_range_controls = new VerticalAxisRangeControls(vertical_axis_state, _vertical_range_controls_section);
        _vertical_range_controls_section->autoSetContentLayout();
        int const insert_index = _horizontal_range_controls_section
                                         ? ui->main_layout->indexOf(_horizontal_range_controls_section) + 1
                                         : (_reference_line_section
                                                    ? ui->main_layout->indexOf(_reference_line_section) + 1
                                                    : 0);
        ui->main_layout->insertWidget(insert_index, _vertical_range_controls_section);
    }
}

void ScatterPlotPropertiesWidget::_updateUIFromState() {
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
            int const idx = _x_key_combo->findText(display);
            if (idx >= 0) {
                _x_key_combo->setCurrentIndex(idx);
                _populateColumnComboBox(_x_column_combo, x_src->data_key);
                // Set the column
                if (x_src->tensor_column_name.has_value()) {
                    int const col_idx = _x_column_combo->findText(
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
            int const idx = _y_key_combo->findText(display);
            if (idx >= 0) {
                _y_key_combo->setCurrentIndex(idx);
                _populateColumnComboBox(_y_column_combo, y_src->data_key);
                if (y_src->tensor_column_name.has_value()) {
                    int const col_idx = _y_column_combo->findText(
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

    // Color by group
    if (_color_by_group_checkbox) {
        _color_by_group_checkbox->setChecked(_state->colorByGroup());
    }

    // Cluster labels
    if (_show_cluster_labels_checkbox) {
        _show_cluster_labels_checkbox->setChecked(_state->showClusterLabels());
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

    // Point coloring
    _updateColorUIFromState();
}

// =============================================================================
// Point Coloring UI
// =============================================================================

namespace {

/// Helper to set a QPushButton background to a hex color string
void setColorButtonStyle(QPushButton * button, std::string const & hex) {
    auto const qhex = QString::fromStdString(hex);
    button->setStyleSheet(
            QStringLiteral("background-color: %1; border: 1px solid #888; min-width: 40px;").arg(qhex));
    button->setText(qhex);
}

/// Determine the PlotSourceRowType for the scatter plot's primary data source
CorePlotting::FeatureColor::PlotSourceRowType inferScatterSourceRowType(
        DataManager & dm,
        ScatterPlotState const & state) {
    // Use the X source to determine source row type (X drives the time base)
    auto const & x_src = state.xSource();
    if (!x_src.has_value() || x_src->data_key.empty()) {
        return CorePlotting::FeatureColor::PlotSourceRowType::Unknown;
    }

    if (dm.getData<AnalogTimeSeries>(x_src->data_key)) {
        return CorePlotting::FeatureColor::PlotSourceRowType::AnalogTimeSeries;
    }
    auto tensor = dm.getData<TensorData>(x_src->data_key);
    if (tensor) {
        switch (tensor->rowType()) {
            case RowType::TimeFrameIndex:
                return CorePlotting::FeatureColor::PlotSourceRowType::TensorTimeFrameIndex;
            case RowType::Interval:
                return CorePlotting::FeatureColor::PlotSourceRowType::TensorInterval;
            case RowType::Ordinal:
                return CorePlotting::FeatureColor::PlotSourceRowType::TensorOrdinal;
        }
    }
    return CorePlotting::FeatureColor::PlotSourceRowType::Unknown;
}

}// namespace

void ScatterPlotPropertiesWidget::_createPointColoringUI() {
    _color_section = new Section(this, "Point Coloring");

    auto * form = new QFormLayout();
    form->setContentsMargins(4, 4, 4, 4);
    form->setSpacing(4);

    // --- Color Mode ---
    _color_mode_combo = new QComboBox();
    _color_mode_combo->addItem("Fixed");
    _color_mode_combo->addItem("By Feature");
    form->addRow("Color Mode:", _color_mode_combo);

    // --- Data Key (feature source) ---
    _color_key_combo = new QComboBox();
    _color_key_combo->setPlaceholderText("Select feature source...");
    form->addRow("Data Key:", _color_key_combo);

    // --- Column (TensorData only) ---
    _color_column_label = new QLabel("Column:");
    _color_column_combo = new QComboBox();
    _color_column_combo->setPlaceholderText("Column");
    form->addRow(_color_column_label, _color_column_combo);

    // --- Compatibility status ---
    _color_compat_label = new QLabel();
    _color_compat_label->setWordWrap(true);
    form->addRow(_color_compat_label);

    // --- Mapping Mode ---
    _mapping_mode_combo = new QComboBox();
    _mapping_mode_combo->addItem("Continuous");
    _mapping_mode_combo->addItem("Threshold");
    form->addRow("Mapping:", _mapping_mode_combo);

    // --- Continuous mode: ColormapControls ---
    _colormap_controls = new ColormapControls();
    form->addRow(_colormap_controls);

    // --- Threshold mode controls ---
    _threshold_label = new QLabel("Threshold:");
    _threshold_spin = new QDoubleSpinBox();
    _threshold_spin->setRange(-1e9, 1e9);
    _threshold_spin->setDecimals(4);
    _threshold_spin->setSingleStep(0.1);
    _threshold_spin->setValue(0.5);
    form->addRow(_threshold_label, _threshold_spin);

    _above_color_label = new QLabel("Above color:");
    _above_color_button = new QPushButton();
    setColorButtonStyle(_above_color_button, "#FF4444");
    form->addRow(_above_color_label, _above_color_button);

    _below_color_label = new QLabel("Below color:");
    _below_color_button = new QPushButton();
    setColorButtonStyle(_below_color_button, "#3388FF");
    form->addRow(_below_color_label, _below_color_button);

    _color_section->setContentLayout(*form);

    // Insert between Glyph Options and Selection
    int const insert_idx = _glyph_style_section
                                   ? ui->main_layout->indexOf(_glyph_style_section) + 1
                                   : ui->main_layout->indexOf(_reference_line_section) + 1;
    ui->main_layout->insertWidget(insert_idx, _color_section);

    // Set initial visibility
    _updateColorUIVisibility();

    // Populate the feature key combo
    _populateColorKeyCombo();

    // --- Connections ---
    connect(_color_mode_combo, &QComboBox::currentIndexChanged,
            this, &ScatterPlotPropertiesWidget::_onColorModeChanged);
    connect(_color_key_combo, &QComboBox::currentIndexChanged,
            this, [this]() {
                if (!_updating_combos) _onColorKeyChanged();
            });
    connect(_color_column_combo, &QComboBox::currentIndexChanged,
            this, [this]() {
                if (!_updating_combos) _onColorColumnChanged();
            });
    connect(_mapping_mode_combo, &QComboBox::currentIndexChanged,
            this, &ScatterPlotPropertiesWidget::_onMappingModeChanged);

    connect(_colormap_controls, &ColormapControls::colormapChanged,
            this, [this]([[maybe_unused]] auto preset) {
                _applyColorConfigToState();
            });
    connect(_colormap_controls, &ColormapControls::colorRangeChanged,
            this, [this]([[maybe_unused]] auto const & config) {
                _applyColorConfigToState();
            });

    connect(_threshold_spin, &QDoubleSpinBox::valueChanged,
            this, [this]([[maybe_unused]] double val) {
                _applyColorConfigToState();
            });

    connect(_above_color_button, &QPushButton::clicked, this, [this]() {
        auto const current = QColor(QString::fromStdString(
                _state ? _state->colorConfig().above_color : "#FF4444"));
        auto const color = QColorDialog::getColor(current, this, "Above Threshold Color");
        if (color.isValid()) {
            setColorButtonStyle(_above_color_button, color.name().toStdString());
            _applyColorConfigToState();
        }
    });

    connect(_below_color_button, &QPushButton::clicked, this, [this]() {
        auto const current = QColor(QString::fromStdString(
                _state ? _state->colorConfig().below_color : "#3388FF"));
        auto const color = QColorDialog::getColor(current, this, "Below Threshold Color");
        if (color.isValid()) {
            setColorButtonStyle(_below_color_button, color.name().toStdString());
            _applyColorConfigToState();
        }
    });
}

void ScatterPlotPropertiesWidget::_populateColorKeyCombo() {
    if (!_color_key_combo || !_data_manager) {
        return;
    }

    _updating_combos = true;
    auto const prev = _color_key_combo->currentText();

    _color_key_combo->clear();
    _color_key_combo->addItem("");// Empty = none

    // Add ATS keys
    for (auto const & key: _data_manager->getKeys<AnalogTimeSeries>()) {
        _color_key_combo->addItem(formatKeyItem(key, ATS_PREFIX));
    }

    // Add TensorData keys
    for (auto const & key: _data_manager->getKeys<TensorData>()) {
        _color_key_combo->addItem(formatKeyItem(key, TD_PREFIX));
    }

    // Add DigitalIntervalSeries keys
    for (auto const & key: _data_manager->getKeys<DigitalIntervalSeries>()) {
        _color_key_combo->addItem(formatKeyItem(key, DIS_PREFIX));
    }

    // Restore previous selection
    int const idx = _color_key_combo->findText(prev);
    if (idx >= 0) {
        _color_key_combo->setCurrentIndex(idx);
    }

    _updating_combos = false;
}

void ScatterPlotPropertiesWidget::_populateColorColumnCombo(std::string const & data_key) {
    if (!_data_manager || data_key.empty()) {
        _color_column_combo->clear();
        _color_column_combo->setVisible(false);
        _color_column_label->setVisible(false);
        return;
    }

    auto tensor = _data_manager->getData<TensorData>(data_key);
    if (!tensor) {
        _color_column_combo->clear();
        _color_column_combo->setVisible(false);
        _color_column_label->setVisible(false);
        return;
    }

    _color_column_combo->setVisible(true);
    _color_column_label->setVisible(true);

    auto const prev_text = _color_column_combo->currentText();
    _color_column_combo->clear();

    auto const & col_names = tensor->columnNames();
    if (!col_names.empty()) {
        for (auto const & name: col_names) {
            _color_column_combo->addItem(QString::fromStdString(name));
        }
    } else {
        auto const num_cols = tensor->numColumns();
        for (std::size_t i = 0; i < num_cols; ++i) {
            _color_column_combo->addItem(QString::number(static_cast<int>(i)));
        }
    }

    int const idx = _color_column_combo->findText(prev_text);
    if (idx >= 0) {
        _color_column_combo->setCurrentIndex(idx);
    } else if (_color_column_combo->count() > 0) {
        _color_column_combo->setCurrentIndex(0);
    }
}

void ScatterPlotPropertiesWidget::_onColorModeChanged(int index) {
    if (_updating_combos) {
        return;
    }
    _updateColorUIVisibility();
    _applyColorConfigToState();
}

void ScatterPlotPropertiesWidget::_onColorKeyChanged() {
    auto key_text = _color_key_combo->currentText();
    if (key_text.isEmpty()) {
        _color_column_combo->clear();
        _color_column_combo->setVisible(false);
        _color_column_label->setVisible(false);
        _color_compat_label->setText("");
        _applyColorConfigToState();
        return;
    }

    auto key = extractKey(key_text);
    _populateColorColumnCombo(key);

    // Auto-switch to Threshold for DigitalIntervalSeries
    if (_data_manager && _data_manager->getData<DigitalIntervalSeries>(key)) {
        _mapping_mode_combo->setCurrentIndex(1);// Threshold
    }

    _applyColorConfigToState();
    _updateColorCompatibilityLabel();
}

void ScatterPlotPropertiesWidget::_onColorColumnChanged() {
    if (_updating_combos) {
        return;
    }
    _applyColorConfigToState();
    _updateColorCompatibilityLabel();
}

void ScatterPlotPropertiesWidget::_onMappingModeChanged(int /*index*/) {
    if (_updating_combos) {
        return;
    }
    _updateColorUIVisibility();
    _applyColorConfigToState();
}

void ScatterPlotPropertiesWidget::_applyColorConfigToState() {
    if (!_state || _updating_combos) {
        return;
    }

    ScatterColorConfigData config;

    // Color mode
    config.color_mode = (_color_mode_combo->currentIndex() == 1) ? "by_feature" : "fixed";

    // Feature source
    auto key_text = _color_key_combo->currentText();
    if (!key_text.isEmpty()) {
        CorePlotting::FeatureColor::FeatureColorSourceDescriptor desc;
        desc.data_key = extractKey(key_text);

        if (_color_column_combo->isVisible() && _color_column_combo->currentIndex() >= 0) {
            auto col_text = _color_column_combo->currentText().toStdString();
            bool is_numeric = false;
            auto col_idx = static_cast<std::size_t>(
                    _color_column_combo->currentText().toInt(&is_numeric));
            if (is_numeric && _data_manager) {
                auto tensor = _data_manager->getData<TensorData>(desc.data_key);
                if (tensor && tensor->columnNames().empty()) {
                    desc.tensor_column_index = col_idx;
                } else {
                    desc.tensor_column_name = col_text;
                }
            } else {
                desc.tensor_column_name = col_text;
            }
        }
        config.color_source = std::move(desc);
    }

    // Mapping mode
    config.mapping_mode = (_mapping_mode_combo->currentIndex() == 1) ? "threshold" : "continuous";

    // Continuous settings
    auto const preset = _colormap_controls->colormapPreset();
    config.colormap_preset = CorePlotting::Colormaps::presetName(preset);
    auto const range = _colormap_controls->colorRange();
    switch (range.mode) {
        case ColorRangeConfig::Mode::Auto:
            config.color_range_mode = "auto";
            break;
        case ColorRangeConfig::Mode::Manual:
            config.color_range_mode = "manual";
            break;
        case ColorRangeConfig::Mode::Symmetric:
            config.color_range_mode = "symmetric";
            break;
    }
    config.color_range_vmin = static_cast<float>(range.vmin);
    config.color_range_vmax = static_cast<float>(range.vmax);

    // Threshold settings
    config.threshold = _threshold_spin->value();
    config.above_color = _above_color_button->text().toStdString();
    config.below_color = _below_color_button->text().toStdString();

    _state->setColorConfig(std::move(config));
}

void ScatterPlotPropertiesWidget::_updateColorCompatibilityLabel() {
    if (!_color_compat_label || !_state || !_data_manager) {
        return;
    }

    auto key_text = _color_key_combo->currentText();
    if (key_text.isEmpty()) {
        _color_compat_label->setText("");
        _color_compat_label->setStyleSheet("");
        return;
    }

    // Build a descriptor from UI
    CorePlotting::FeatureColor::FeatureColorSourceDescriptor desc;
    desc.data_key = extractKey(key_text);
    if (_color_column_combo->isVisible() && _color_column_combo->currentIndex() >= 0) {
        desc.tensor_column_name = _color_column_combo->currentText().toStdString();
    }

    auto const source_type = inferScatterSourceRowType(*_data_manager, *_state);
    auto const result = CorePlotting::FeatureColor::checkFeatureColorCompatibility(
            *_data_manager, desc, source_type, 0);

    if (result.compatible) {
        _color_compat_label->setText(QStringLiteral("\u2713 Compatible"));
        _color_compat_label->setStyleSheet("color: green;");
    } else {
        _color_compat_label->setText(QString::fromStdString(result.reason));
        _color_compat_label->setStyleSheet("color: red;");
    }
}

void ScatterPlotPropertiesWidget::_updateColorUIFromState() {
    if (!_state) {
        return;
    }

    _updating_combos = true;

    auto const & cc = _state->colorConfig();

    // Color mode
    _color_mode_combo->setCurrentIndex(cc.color_mode == "by_feature" ? 1 : 0);

    // Feature source
    if (cc.color_source.has_value() && !cc.color_source->data_key.empty() && _data_manager) {
        auto const & key = cc.color_source->data_key;
        // Determine prefix
        char const * prefix = ATS_PREFIX;
        if (_data_manager->getData<TensorData>(key)) {
            prefix = TD_PREFIX;
        } else if (_data_manager->getData<DigitalIntervalSeries>(key)) {
            prefix = DIS_PREFIX;
        }
        auto const display = formatKeyItem(key, prefix);
        int const idx = _color_key_combo->findText(display);
        if (idx >= 0) {
            _color_key_combo->setCurrentIndex(idx);
        }

        _populateColorColumnCombo(key);
        if (cc.color_source->tensor_column_name.has_value()) {
            int const col_idx = _color_column_combo->findText(
                    QString::fromStdString(*cc.color_source->tensor_column_name));
            if (col_idx >= 0) {
                _color_column_combo->setCurrentIndex(col_idx);
            }
        } else if (cc.color_source->tensor_column_index.has_value()) {
            _color_column_combo->setCurrentIndex(
                    static_cast<int>(*cc.color_source->tensor_column_index));
        }
    }

    // Mapping mode
    _mapping_mode_combo->setCurrentIndex(cc.mapping_mode == "threshold" ? 1 : 0);

    // Continuous settings
    auto const preset = CorePlotting::Colormaps::presetFromName(cc.colormap_preset);
    _colormap_controls->setColormapPreset(preset);

    ColorRangeConfig range_config;
    if (cc.color_range_mode == "manual") {
        range_config.mode = ColorRangeConfig::Mode::Manual;
    } else if (cc.color_range_mode == "symmetric") {
        range_config.mode = ColorRangeConfig::Mode::Symmetric;
    } else {
        range_config.mode = ColorRangeConfig::Mode::Auto;
    }
    range_config.vmin = static_cast<double>(cc.color_range_vmin);
    range_config.vmax = static_cast<double>(cc.color_range_vmax);
    _colormap_controls->setColorRange(range_config);

    // Threshold settings
    _threshold_spin->setValue(cc.threshold);
    setColorButtonStyle(_above_color_button, cc.above_color);
    setColorButtonStyle(_below_color_button, cc.below_color);

    _updating_combos = false;

    _updateColorUIVisibility();
    _updateColorCompatibilityLabel();
}

void ScatterPlotPropertiesWidget::_updateColorUIVisibility() {
    bool const by_feature = _color_mode_combo->currentIndex() == 1;
    bool const continuous = _mapping_mode_combo->currentIndex() == 0;

    // Feature source controls visible only in "By Feature" mode
    _color_key_combo->setVisible(by_feature);
    // Column only if by_feature AND the key is TensorData (handled by _populateColorColumnCombo)
    _color_column_combo->setVisible(by_feature && _color_column_combo->count() > 0);
    _color_column_label->setVisible(by_feature && _color_column_combo->count() > 0);
    _color_compat_label->setVisible(by_feature);
    _mapping_mode_combo->setVisible(by_feature);

    // Continuous vs threshold controls
    _colormap_controls->setVisible(by_feature && continuous);
    _threshold_label->setVisible(by_feature && !continuous);
    _threshold_spin->setVisible(by_feature && !continuous);
    _above_color_label->setVisible(by_feature && !continuous);
    _above_color_button->setVisible(by_feature && !continuous);
    _below_color_label->setVisible(by_feature && !continuous);
    _below_color_button->setVisible(by_feature && !continuous);
}
