#include "ColumnConfigDialog.hpp"

#include "TensorDesigner.hpp"// For DesignerRowType

#include "DataManager/DataManager.hpp"

//https://stackoverflow.com/questions/72533139/libtorch-errors-when-used-with-qt-opencv-and-point-cloud-library
#undef slots
#include "TransformsV2/core/TensorColumnBuilders.hpp"
#define slots Q_SLOTS

#include <nlohmann/json.hpp>

//https://stackoverflow.com/questions/72533139/libtorch-errors-when-used-with-qt-opencv-and-point-cloud-library
#undef slots
#include "TransformsV2/core/PipelineLoader.hpp"
#define slots Q_SLOTS

#include "EditorState/OperationContext.hpp"

#include <QComboBox>
#include <QDialogButtonBox>
#include <QFormLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QTextEdit>
#include <QVBoxLayout>

using namespace WhiskerToolbox::TensorBuilders;

// =============================================================================
// Construction
// =============================================================================

ColumnConfigDialog::ColumnConfigDialog(
        std::shared_ptr<DataManager> data_manager,
        DesignerRowType row_type,
        EditorLib::OperationContext * operation_context,
        QWidget * parent)
    : QDialog(parent),
      _data_manager(std::move(data_manager)),
      _row_type(row_type),
      _operation_context(operation_context),
      _requester_id(EditorLib::EditorInstanceId::generate().toString()) {
    _setupUi();
    _connectSignals();
    _populateSourceKeys();
}

ColumnConfigDialog::ColumnConfigDialog(
        std::shared_ptr<DataManager> data_manager,
        DesignerRowType row_type,
        ColumnRecipe const & recipe,
        EditorLib::OperationContext * operation_context,
        QWidget * parent)
    : QDialog(parent),
      _data_manager(std::move(data_manager)),
      _row_type(row_type),
      _operation_context(operation_context),
      _requester_id(EditorLib::EditorInstanceId::generate().toString()) {
    _setupUi();
    _connectSignals();
    _populateSourceKeys();
    _applyRecipe(recipe);
}

ColumnConfigDialog::~ColumnConfigDialog() {
    _cleanupPendingOperation();
}

// =============================================================================
// Public API
// =============================================================================

ColumnRecipe ColumnConfigDialog::getRecipe() const {
    ColumnRecipe recipe;
    recipe.column_name = _name_edit->text().toStdString();

    // Source key
    int const source_idx = _source_combo->currentIndex();
    if (source_idx >= 0) {
        recipe.source_key = _source_combo->currentData().toString().toStdString();
    }

    // Interval property takes priority — no pipeline needed
    if (_interval_property_group && _interval_property_group->isChecked()) {
        int const prop_idx = _interval_property_combo->currentIndex();
        switch (prop_idx) {
            case 0:
                recipe.interval_property = IntervalProperty::Start;
                break;
            case 1:
                recipe.interval_property = IntervalProperty::End;
                break;
            case 2:
                recipe.interval_property = IntervalProperty::Duration;
                break;
            default:
                break;
        }
        recipe.pipeline_json.clear();
        return recipe;
    }

    // Pipeline JSON
    recipe.pipeline_json = _pipeline_json_edit->toPlainText().trimmed().toStdString();

    return recipe;
}

// =============================================================================
// Slots
// =============================================================================

void ColumnConfigDialog::_onSourceKeyChanged(int /*index*/) {
    // Update the source type label
    int const source_idx = _source_combo->currentIndex();
    if (source_idx >= 0 && _data_manager) {
        auto const source_key = _source_combo->currentData().toString().toStdString();
        auto const source_type = _data_manager->getType(source_key);
        _source_type_label->setText(
                QString::fromStdString(convert_data_type_to_string(source_type)));
    } else {
        _source_type_label->clear();
    }
    _updateAutoName();
}

void ColumnConfigDialog::_onColumnNameEdited(QString const & /*text*/) {
    _auto_name = false;// User typed a custom name
}

void ColumnConfigDialog::_updateAutoName() {
    if (!_auto_name) {
        return;
    }

    QString name;
    int const source_idx = _source_combo->currentIndex();
    if (source_idx >= 0) {
        name = _source_combo->currentData().toString();
    }

    // If interval property is active, append property name
    if (_interval_property_group && _interval_property_group->isChecked()) {
        if (!name.isEmpty()) name += QStringLiteral("_");
        name += _interval_property_combo->currentText();
    }

    name.replace(QStringLiteral(" "), QStringLiteral("_"));

    _name_edit->blockSignals(true);
    _name_edit->setText(name);
    _name_edit->blockSignals(false);
}

void ColumnConfigDialog::_onIntervalPropertyToggled(bool checked) {
    // When interval property is checked, the source/pipeline sections
    // become irrelevant (but remain visible for context)
    _source_combo->setEnabled(!checked);
    _pipeline_json_edit->setEnabled(!checked);
    _validate_btn->setEnabled(!checked);
    _request_tv2_btn->setEnabled(!checked && _operation_context != nullptr);
    _updateAutoName();
}

// =============================================================================
// Setup
// =============================================================================

void ColumnConfigDialog::_setupUi() {
    setWindowTitle(QStringLiteral("Configure Column"));
    setMinimumWidth(450);

    _layout = new QVBoxLayout(this);

    // --- Source selection ---
    auto * source_group = new QGroupBox(QStringLiteral("Data Source"), this);
    auto * source_layout = new QFormLayout(source_group);

    _source_combo = new QComboBox(source_group);
    _source_combo->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    source_layout->addRow(QStringLiteral("Source Key:"), _source_combo);

    _source_type_label = new QLabel(source_group);
    _source_type_label->setStyleSheet(QStringLiteral("color: gray;"));
    source_layout->addRow(QStringLiteral("Type:"), _source_type_label);

    _layout->addWidget(source_group);

    // --- Pipeline JSON (primary column configuration) ---
    auto * pipeline_group = new QGroupBox(QStringLiteral("Pipeline"), this);
    auto * pipeline_layout = new QVBoxLayout(pipeline_group);

    _pipeline_json_edit = new QTextEdit(pipeline_group);
    _pipeline_json_edit->setPlaceholderText(
            QStringLiteral("Pipeline JSON (empty = passthrough).\n"
                           "Use 'Request from Transforms V2' to build a pipeline,\n"
                           "or type JSON directly.\n\n"
                           "Example:\n"
                           "{\n"
                           "  \"range_reduction\": {\n"
                           "    \"name\": \"MeanValue\",\n"
                           "    \"parameters\": {}\n"
                           "  }\n"
                           "}"));
    _pipeline_json_edit->setAcceptRichText(false);
    _pipeline_json_edit->setTabChangesFocus(false);
    auto font = _pipeline_json_edit->font();
    font.setFamily(QStringLiteral("monospace"));
    font.setPointSize(9);
    _pipeline_json_edit->setFont(font);
    _pipeline_json_edit->setMaximumHeight(200);
    pipeline_layout->addWidget(_pipeline_json_edit);

    auto * btn_layout = new QHBoxLayout();
    _request_tv2_btn = new QPushButton(
            QStringLiteral("Request from Transforms V2"), pipeline_group);
    _request_tv2_btn->setToolTip(
            QStringLiteral("Open the Transforms V2 widget and request a pipeline. "
                           "Configure your pipeline there, then click 'Send Pipeline' "
                           "to deliver it here."));
    _request_tv2_btn->setEnabled(_operation_context != nullptr);
    btn_layout->addWidget(_request_tv2_btn);

    _validate_btn = new QPushButton(QStringLiteral("Validate"), pipeline_group);
    btn_layout->addWidget(_validate_btn);
    pipeline_layout->addLayout(btn_layout);

    _validation_label = new QLabel(pipeline_group);
    _validation_label->setWordWrap(true);
    pipeline_layout->addWidget(_validation_label);

    _layout->addWidget(pipeline_group);

    // --- Interval property section (only for interval row type) ---
    if (_row_type == DesignerRowType::Interval) {
        _interval_property_group = new QGroupBox(
                QStringLiteral("Interval Property Column"), this);
        _interval_property_group->setCheckable(true);
        _interval_property_group->setChecked(false);
        _interval_property_group->setToolTip(
                QStringLiteral("Extract a property from the row intervals themselves "
                               "(start time, end time, or duration). No data source or "
                               "pipeline is needed for interval property columns."));
        auto * ip_layout = new QFormLayout(_interval_property_group);

        _interval_property_combo = new QComboBox(_interval_property_group);
        _interval_property_combo->addItem(QStringLiteral("Start"));
        _interval_property_combo->addItem(QStringLiteral("End"));
        _interval_property_combo->addItem(QStringLiteral("Duration"));
        ip_layout->addRow(QStringLiteral("Property:"), _interval_property_combo);

        _layout->addWidget(_interval_property_group);
    }

    // --- Column name ---
    auto * name_group = new QGroupBox(QStringLiteral("Column Name"), this);
    auto * name_layout = new QFormLayout(name_group);

    _name_edit = new QLineEdit(name_group);
    _name_edit->setPlaceholderText(QStringLiteral("Auto-generated from source key"));
    name_layout->addRow(QStringLiteral("Name:"), _name_edit);

    _layout->addWidget(name_group);

    // --- Dialog buttons ---
    auto * button_box = new QDialogButtonBox(
            QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
    connect(button_box, &QDialogButtonBox::accepted, this, &QDialog::accept);
    connect(button_box, &QDialogButtonBox::rejected, this, &QDialog::reject);
    _layout->addWidget(button_box);
}

void ColumnConfigDialog::_connectSignals() {
    connect(_source_combo, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &ColumnConfigDialog::_onSourceKeyChanged);
    connect(_name_edit, &QLineEdit::textEdited,
            this, &ColumnConfigDialog::_onColumnNameEdited);
    connect(_validate_btn, &QPushButton::clicked,
            this, &ColumnConfigDialog::_onValidateClicked);
    connect(_request_tv2_btn, &QPushButton::clicked,
            this, &ColumnConfigDialog::_onRequestTV2Clicked);

    // Interval property
    if (_interval_property_group) {
        connect(_interval_property_group, &QGroupBox::toggled,
                this, &ColumnConfigDialog::_onIntervalPropertyToggled);
    }

    // OperationContext delivery and close signals
    if (_operation_context) {
        connect(_operation_context, &EditorLib::OperationContext::operationDelivered,
                this, &ColumnConfigDialog::_onOperationDelivered);
        connect(_operation_context, &EditorLib::OperationContext::operationClosed,
                this, &ColumnConfigDialog::_onOperationClosed);
    }
}

void ColumnConfigDialog::_populateSourceKeys() {
    _source_combo->blockSignals(true);
    _source_combo->clear();

    if (!_data_manager) {
        _source_combo->blockSignals(false);
        return;
    }

    auto all_keys = _data_manager->getAllKeys();
    for (auto const & key: all_keys) {
        auto const type = _data_manager->getType(key);

        // Show all concrete data types that are valid column sources.
        bool compatible = (type != DM_DataType::Unknown &&
                           type != DM_DataType::Time &&
                           type != DM_DataType::Tensor);

        if (compatible) {
            auto type_str = QString::fromStdString(convert_data_type_to_string(type));
            auto display = QString::fromStdString(key) +
                           QStringLiteral(" [") + type_str + QStringLiteral("]");
            _source_combo->addItem(display, QString::fromStdString(key));
        }
    }

    _source_combo->blockSignals(false);

    if (_source_combo->count() > 0) {
        _onSourceKeyChanged(0);
    }
}

void ColumnConfigDialog::_applyRecipe(ColumnRecipe const & recipe) {
    _auto_name = false;

    // Set source key
    for (int i = 0; i < _source_combo->count(); ++i) {
        if (_source_combo->itemData(i).toString().toStdString() == recipe.source_key) {
            _source_combo->setCurrentIndex(i);
            break;
        }
    }

    // Set pipeline JSON
    if (!recipe.pipeline_json.empty()) {
        try {
            auto j = nlohmann::json::parse(recipe.pipeline_json);
            _pipeline_json_edit->setPlainText(
                    QString::fromStdString(j.dump(2)));
        } catch (...) {
            _pipeline_json_edit->setPlainText(
                    QString::fromStdString(recipe.pipeline_json));
        }
    }

    // Set interval property
    if (recipe.interval_property.has_value() && _interval_property_group) {
        _interval_property_group->setChecked(true);
        switch (recipe.interval_property.value()) {
            case IntervalProperty::Start:
                _interval_property_combo->setCurrentIndex(0);
                break;
            case IntervalProperty::End:
                _interval_property_combo->setCurrentIndex(1);
                break;
            case IntervalProperty::Duration:
                _interval_property_combo->setCurrentIndex(2);
                break;
        }
    }

    // Set column name
    _name_edit->setText(QString::fromStdString(recipe.column_name));
}

// =============================================================================
// Pipeline Validation
// =============================================================================

void ColumnConfigDialog::_onValidateClicked() {
    auto const json_text = _pipeline_json_edit->toPlainText().trimmed().toStdString();
    if (json_text.empty()) {
        _validation_label->setStyleSheet(QStringLiteral("color: green;"));
        _validation_label->setText(
                QStringLiteral("\u2713 Empty pipeline (passthrough / direct value)"));
        return;
    }

    // Check if it's valid JSON
    try {
        nlohmann::json::parse(json_text);
    } catch (nlohmann::json::parse_error const & e) {
        _validation_label->setStyleSheet(QStringLiteral("color: red;"));
        _validation_label->setText(
                QStringLiteral("JSON parse error: ") +
                QString::fromStdString(e.what()));
        return;
    }

    // Try to load as a TransformPipeline
    auto result = WhiskerToolbox::Transforms::V2::Examples::loadPipelineFromJson(json_text);
    if (result) {
        auto const & pipeline = result.value();
        QString detail;
        auto const step_count = pipeline.size();
        auto const & rr = pipeline.getRangeReduction();
        if (step_count > 0) {
            detail += QString::number(step_count) +
                      QStringLiteral(" transform step(s)");
        }
        if (rr.has_value()) {
            if (!detail.isEmpty()) detail += QStringLiteral(", ");
            detail += QStringLiteral("range reduction: ") +
                      QString::fromStdString(rr->reduction_name);
        }
        if (detail.isEmpty()) {
            detail = QStringLiteral("empty pipeline (passthrough)");
        }
        _validation_label->setStyleSheet(QStringLiteral("color: green;"));
        _validation_label->setText(
                QStringLiteral("\u2713 Valid pipeline: ") + detail);
    } else {
        _validation_label->setStyleSheet(QStringLiteral("color: red;"));
        _validation_label->setText(
                QStringLiteral("Pipeline load error: ") +
                QString::fromStdString(result.error()->what()));
    }
}

// =============================================================================
// TransformsV2 Integration
// =============================================================================

void ColumnConfigDialog::_onRequestTV2Clicked() {
    if (!_operation_context) {
        _validation_label->setStyleSheet(QStringLiteral("color: red;"));
        _validation_label->setText(
                QStringLiteral("OperationContext not available. Cannot request pipeline."));
        return;
    }

    // Clean up any previous request
    _cleanupPendingOperation();

    // Request a pipeline from TransformsV2Widget
    auto result = _operation_context->requestOperation(
            EditorLib::EditorInstanceId(_requester_id),
            EditorLib::EditorTypeId(QStringLiteral("TransformsV2Widget")),
            EditorLib::OperationRequestOptions{
                    .channel = EditorLib::DataChannels::TransformPipeline,
                    .close_on_selection_change = false,
                    .close_after_delivery = true});

    if (result.has_value()) {
        _pending_operation_id = result->id.toString();
        _request_tv2_btn->setText(QStringLiteral("Waiting for pipeline..."));
        _request_tv2_btn->setEnabled(false);
        _validation_label->setStyleSheet(QStringLiteral("color: #0066cc;"));
        _validation_label->setText(
                QStringLiteral("Requested pipeline from Transforms V2 widget. "
                               "Configure your pipeline there, then click 'Send Pipeline'."));
    } else {
        _validation_label->setStyleSheet(QStringLiteral("color: red;"));
        _validation_label->setText(
                QStringLiteral("Failed to request pipeline from Transforms V2 widget."));
    }
}

void ColumnConfigDialog::_onOperationDelivered(
        EditorLib::PendingOperation const & op,
        EditorLib::OperationResult const & result) {

    // Check if this delivery is for our request
    if (op.requester.toString() != _requester_id) {
        return;
    }

    // Extract the envelope JSON from the result
    auto const * json_ptr = result.peek<std::string>();
    if (!json_ptr) {
        _validation_label->setStyleSheet(QStringLiteral("color: red;"));
        _validation_label->setText(
                QStringLiteral("Received result but could not extract pipeline JSON."));
        _resetRequestButton();
        return;
    }

    // Parse the envelope — it may contain { pipeline, input_key, input_type }
    std::string pipeline_json_str;
    std::string input_key;
    std::string input_type;
    try {
        auto envelope = nlohmann::json::parse(*json_ptr);
        if (envelope.contains("pipeline")) {
            auto const & pipe = envelope["pipeline"];
            pipeline_json_str = pipe.is_string()
                    ? pipe.get<std::string>()
                    : pipe.dump();
        } else {
            // Fallback: treat entire string as pipeline JSON
            pipeline_json_str = *json_ptr;
        }
        if (envelope.contains("input_key")) {
            input_key = envelope["input_key"].get<std::string>();
        }
        if (envelope.contains("input_type")) {
            input_type = envelope["input_type"].get<std::string>();
        }
    } catch (...) {
        // If envelope parsing fails, treat entire string as pipeline JSON
        pipeline_json_str = *json_ptr;
    }

    // Auto-select the source key from the delivered metadata
    if (!input_key.empty()) {
        bool found = false;
        for (int i = 0; i < _source_combo->count(); ++i) {
            if (_source_combo->itemData(i).toString().toStdString() == input_key) {
                _source_combo->setCurrentIndex(i);
                found = true;
                break;
            }
        }
        if (!found) {
            // Key not in combo — add it dynamically
            QString display_type;
            if (!input_type.empty()) {
                display_type = QString::fromStdString(input_type);
            } else if (_data_manager) {
                display_type = QString::fromStdString(
                        convert_data_type_to_string(_data_manager->getType(input_key)));
            }
            auto display = QString::fromStdString(input_key) +
                           QStringLiteral(" [") + display_type + QStringLiteral("]");
            _source_combo->addItem(display, QString::fromStdString(input_key));
            _source_combo->setCurrentIndex(_source_combo->count() - 1);
        }
    }

    // Populate the pipeline text edit
    try {
        auto j = nlohmann::json::parse(pipeline_json_str);
        _pipeline_json_edit->setPlainText(
                QString::fromStdString(j.dump(2)));
    } catch (...) {
        _pipeline_json_edit->setPlainText(
                QString::fromStdString(pipeline_json_str));
    }

    // Auto-validate the received pipeline
    _onValidateClicked();

    // Reset the button and clear the pending operation ID
    _pending_operation_id.clear();
    _resetRequestButton();
}

void ColumnConfigDialog::_onOperationClosed(EditorLib::OperationId const & id) {
    if (id.toString() != _pending_operation_id) {
        return;
    }
    _pending_operation_id.clear();
    _resetRequestButton();
}

void ColumnConfigDialog::_cleanupPendingOperation() {
    if (!_pending_operation_id.isEmpty() && _operation_context) {
        _operation_context->closeOperation(
                EditorLib::OperationId(_pending_operation_id),
                EditorLib::OperationCloseReason::RequesterClosed);
        _pending_operation_id.clear();
    }
}

void ColumnConfigDialog::_resetRequestButton() {
    if (_request_tv2_btn) {
        _request_tv2_btn->setText(QStringLiteral("Request from Transforms V2"));
        _request_tv2_btn->setEnabled(_operation_context != nullptr);
    }
}
