/**
 * @file DataSynthesizerProperties_Widget.cpp
 * @brief Implementation of the DataSynthesizer properties panel
 */

#include "DataSynthesizerProperties_Widget.hpp"

#include "DataSynthesizer_Widget/Core/DataSynthesizerState.hpp"

#include "AutoParamWidget/AutoParamWidget.hpp"
#include "DataManager/DataManager.hpp"
#include "DataSynthesizer/GeneratorRegistry.hpp"

#include "AnalogTimeSeries/Analog_Time_Series.hpp"
#include "Commands/Core/CommandContext.hpp"
#include "Commands/Core/SequenceExecution.hpp"
#include "Commands/SynthesizeData.hpp"

#include <rfl.hpp>
#include <rfl/json.hpp>

#include <QComboBox>
#include <QFormLayout>
#include <QGroupBox>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QScrollArea>
#include <QVBoxLayout>

#include <algorithm>
#include <iostream>

DataSynthesizerProperties_Widget::DataSynthesizerProperties_Widget(
        std::shared_ptr<DataSynthesizerState> state,
        std::shared_ptr<DataManager> data_manager,
        QWidget * parent)
    : QWidget(parent),
      _state(std::move(state)),
      _data_manager(std::move(data_manager)) {

    auto * main_layout = new QVBoxLayout(this);

    // --- Title ---
    auto * title_label = new QLabel(QStringLiteral("Data Synthesizer"), this);
    title_label->setAlignment(Qt::AlignCenter);
    auto title_font = title_label->font();
    title_font.setBold(true);
    title_font.setPointSize(title_font.pointSize() + 2);
    title_label->setFont(title_font);
    main_layout->addWidget(title_label);

    // --- Generator Selection ---
    auto * selection_group = new QGroupBox(QStringLiteral("Generator Selection"), this);
    auto * selection_layout = new QFormLayout(selection_group);

    _output_type_combo = new QComboBox(selection_group);
    selection_layout->addRow(QStringLiteral("Output Type:"), _output_type_combo);

    _generator_combo = new QComboBox(selection_group);
    selection_layout->addRow(QStringLiteral("Generator:"), _generator_combo);

    main_layout->addWidget(selection_group);

    // --- Parameters (scrollable) ---
    auto * params_group = new QGroupBox(QStringLiteral("Parameters"), this);
    auto * params_outer_layout = new QVBoxLayout(params_group);

    auto * scroll_area = new QScrollArea(params_group);
    scroll_area->setWidgetResizable(true);
    scroll_area->setFrameShape(QFrame::NoFrame);

    _auto_param_widget = new AutoParamWidget(scroll_area);
    scroll_area->setWidget(_auto_param_widget);

    params_outer_layout->addWidget(scroll_area);
    main_layout->addWidget(params_group);

    // --- Output Configuration ---
    auto * output_group = new QGroupBox(QStringLiteral("Output"), this);
    auto * output_layout = new QFormLayout(output_group);

    _output_key_edit = new QLineEdit(output_group);
    _output_key_edit->setPlaceholderText(QStringLiteral("e.g. sine_wave_1"));
    output_layout->addRow(QStringLiteral("Data Key:"), _output_key_edit);

    _time_key_edit = new QLineEdit(output_group);
    _time_key_edit->setPlaceholderText(QStringLiteral("auto: <data_key>_time"));
    output_layout->addRow(QStringLiteral("Time Key:"), _time_key_edit);

    _time_frame_mode_combo = new QComboBox(output_group);
    _time_frame_mode_combo->addItem(QStringLiteral("Create New"), QStringLiteral("create_new"));
    _time_frame_mode_combo->addItem(QStringLiteral("Use Existing"), QStringLiteral("use_existing"));
    _time_frame_mode_combo->addItem(QStringLiteral("Overwrite"), QStringLiteral("overwrite"));
    output_layout->addRow(QStringLiteral("TimeFrame:"), _time_frame_mode_combo);

    main_layout->addWidget(output_group);

    // --- Preview & Generate Buttons ---
    auto * preview_btn = new QPushButton(QStringLiteral("Preview"), this);
    main_layout->addWidget(preview_btn);

    auto * generate_btn = new QPushButton(QStringLiteral("Generate"), this);
    main_layout->addWidget(generate_btn);

    // --- Status Label ---
    _status_label = new QLabel(this);
    _status_label->setWordWrap(true);
    main_layout->addWidget(_status_label);

    main_layout->addStretch();

    // --- Connections ---
    connect(_output_type_combo, &QComboBox::currentIndexChanged,
            this, &DataSynthesizerProperties_Widget::_onOutputTypeChanged);
    connect(_generator_combo, &QComboBox::currentIndexChanged,
            this, &DataSynthesizerProperties_Widget::_onGeneratorChanged);
    connect(preview_btn, &QPushButton::clicked,
            this, &DataSynthesizerProperties_Widget::_onPreviewClicked);
    connect(generate_btn, &QPushButton::clicked,
            this, &DataSynthesizerProperties_Widget::_onGenerateClicked);

    connect(_auto_param_widget, &AutoParamWidget::parametersChanged, this, [this]() {
        if (!_restoring) {
            _state->setParameterJson(_auto_param_widget->toJson());
        }
    });

    connect(_output_key_edit, &QLineEdit::textChanged, this, [this](QString const & text) {
        if (!_restoring) {
            _state->setOutputKey(text.toStdString());
        }
    });

    connect(_time_key_edit, &QLineEdit::textChanged, this, [this](QString const & text) {
        if (!_restoring) {
            _state->setTimeKey(text.toStdString());
        }
    });

    connect(_time_frame_mode_combo, &QComboBox::currentIndexChanged, this, [this](int idx) {
        if (!_restoring && idx >= 0) {
            _state->setTimeFrameMode(
                    _time_frame_mode_combo->currentData().toString().toStdString());
        }
    });

    // --- Populate and restore ---
    _populateOutputTypes();
    _restoreFromState();
}

void DataSynthesizerProperties_Widget::_onOutputTypeChanged(int index) {
    if (_restoring || index < 0) {
        return;
    }
    auto const output_type = _output_type_combo->currentText().toStdString();
    _state->setOutputType(output_type);
    _populateGenerators(output_type);
}

void DataSynthesizerProperties_Widget::_onGeneratorChanged(int index) {
    if (_restoring || index < 0) {
        return;
    }
    auto const generator_name = _generator_combo->currentText().toStdString();
    _state->setGeneratorName(generator_name);
    _setupGeneratorParams(generator_name);

    // Suggest a default output key if empty
    if (_output_key_edit->text().isEmpty()) {
        _output_key_edit->setText(QString::fromStdString(generator_name));
    }
}

void DataSynthesizerProperties_Widget::_onPreviewClicked() {
    auto const & generator_name = _state->generatorName();
    if (generator_name.empty()) {
        _status_label->setText(QStringLiteral("Please select a generator."));
        return;
    }

    auto const & param_json_str = _state->parameterJson();
    auto const & registry = WhiskerToolbox::DataSynthesizer::GeneratorRegistry::instance();

    auto result = registry.generate(generator_name, param_json_str);
    if (!result) {
        _status_label->setText(QStringLiteral("Preview failed: generator returned no data."));
        return;
    }

    auto const * analog_ptr = std::get_if<std::shared_ptr<AnalogTimeSeries>>(&*result);
    if (!analog_ptr || !*analog_ptr) {
        _status_label->setText(QStringLiteral("Preview is only available for AnalogTimeSeries generators."));
        return;
    }

    _status_label->setText(QStringLiteral("Preview updated."));
    emit previewRequested(*analog_ptr);
}

void DataSynthesizerProperties_Widget::_onGenerateClicked() {
    auto const & generator_name = _state->generatorName();
    auto const & output_key = _state->outputKey();

    if (generator_name.empty()) {
        _status_label->setText(QStringLiteral("Please select a generator."));
        return;
    }
    if (output_key.empty()) {
        _status_label->setText(QStringLiteral("Please enter an output data key."));
        return;
    }

    auto const & param_json_str = _state->parameterJson();

    // Parse parameter JSON into rfl::Generic for the command
    auto parsed = rfl::json::read<rfl::Generic>(param_json_str);
    if (!parsed) {
        _status_label->setText(QStringLiteral("Invalid parameter JSON."));
        return;
    }

    commands::SynthesizeDataParams cmd_params;
    cmd_params.output_key = output_key;
    cmd_params.generator_name = generator_name;
    cmd_params.output_type = _state->outputType();
    cmd_params.parameters = std::move(*parsed);
    cmd_params.time_frame_mode = _state->timeFrameMode();

    // Pass time key if the user specified one; otherwise leave as nullopt
    // so the command auto-generates "<output_key>_time".
    auto const & tk = _state->timeKey();
    if (!tk.empty()) {
        cmd_params.time_key = tk;
    }

    auto const params_json = rfl::json::write(cmd_params);

    commands::CommandContext ctx;
    ctx.data_manager = _data_manager;
    ctx.recorder = _command_recorder;

    auto result = commands::executeSingleCommand("SynthesizeData", params_json, ctx);
    if (result.success) {
        _status_label->setText(
                QStringLiteral("Generated '%1' successfully.")
                        .arg(QString::fromStdString(output_key)));
    } else {
        _status_label->setText(
                QStringLiteral("Error: %1")
                        .arg(QString::fromStdString(result.error_message)));
    }
}

void DataSynthesizerProperties_Widget::_populateOutputTypes() {
    auto const & registry = WhiskerToolbox::DataSynthesizer::GeneratorRegistry::instance();
    auto const output_types = registry.listOutputTypes();

    _output_type_combo->blockSignals(true);
    _output_type_combo->clear();
    for (auto const & type: output_types) {
        _output_type_combo->addItem(QString::fromStdString(type));
    }
    _output_type_combo->blockSignals(false);
}

void DataSynthesizerProperties_Widget::_populateGenerators(std::string const & output_type) {
    auto const & registry = WhiskerToolbox::DataSynthesizer::GeneratorRegistry::instance();
    auto const generators = registry.listGenerators(output_type);

    _generator_combo->blockSignals(true);
    _generator_combo->clear();
    for (auto const & name: generators) {
        _generator_combo->addItem(QString::fromStdString(name));
    }
    _generator_combo->blockSignals(false);

    // Auto-select first generator if available
    if (!generators.empty()) {
        _generator_combo->setCurrentIndex(0);
        _onGeneratorChanged(0);
    } else {
        _auto_param_widget->clear();
    }
}

void DataSynthesizerProperties_Widget::_setupGeneratorParams(std::string const & generator_name) {
    auto const & registry = WhiskerToolbox::DataSynthesizer::GeneratorRegistry::instance();
    auto const schema = registry.getSchema(generator_name);

    if (schema) {
        _auto_param_widget->setSchema(*schema);
        _state->setParameterJson(_auto_param_widget->toJson());
    } else {
        _auto_param_widget->clear();
    }
}

void DataSynthesizerProperties_Widget::_restoreFromState() {
    _restoring = true;

    auto const & output_type = _state->outputType();
    auto const & generator_name = _state->generatorName();
    auto const & param_json = _state->parameterJson();
    auto const & output_key = _state->outputKey();

    // Restore output type selection
    if (!output_type.empty()) {
        int const idx = _output_type_combo->findText(QString::fromStdString(output_type));
        if (idx >= 0) {
            _output_type_combo->setCurrentIndex(idx);
            _populateGenerators(output_type);
        }
    } else if (_output_type_combo->count() > 0) {
        // Default to first output type — also sync state
        _output_type_combo->setCurrentIndex(0);
        auto const default_output_type = _output_type_combo->currentText().toStdString();
        _state->setOutputType(default_output_type);
        _populateGenerators(default_output_type);
    }

    // Restore generator selection
    if (!generator_name.empty()) {
        int const idx = _generator_combo->findText(QString::fromStdString(generator_name));
        if (idx >= 0) {
            _generator_combo->setCurrentIndex(idx);
            _setupGeneratorParams(generator_name);
        }
    } else if (_generator_combo->count() > 0) {
        // First load with no saved state: sync selected generator to state
        auto const default_generator = _generator_combo->currentText().toStdString();
        _state->setGeneratorName(default_generator);
        _setupGeneratorParams(default_generator);
    }

    // Restore parameters
    if (!param_json.empty()) {
        _auto_param_widget->fromJson(param_json);
    }

    // Restore output key
    if (!output_key.empty()) {
        _output_key_edit->setText(QString::fromStdString(output_key));
    }

    // Restore time key
    auto const & time_key = _state->timeKey();
    if (!time_key.empty()) {
        _time_key_edit->setText(QString::fromStdString(time_key));
    }

    // Restore time frame mode
    auto const & mode = _state->timeFrameMode();
    int const mode_idx = _time_frame_mode_combo->findData(QString::fromStdString(mode));
    if (mode_idx >= 0) {
        _time_frame_mode_combo->setCurrentIndex(mode_idx);
    }

    _restoring = false;
}
