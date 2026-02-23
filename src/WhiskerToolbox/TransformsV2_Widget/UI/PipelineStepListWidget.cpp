#include "PipelineStepListWidget.hpp"

#include "TransformsV2/core/ElementRegistry.hpp"
#include "TransformsV2/core/PipelineLoader.hpp"
#include "TransformsV2/core/TypeChainResolver.hpp"

#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QPushButton>
#include <QTableWidget>
#include <QVBoxLayout>

#include <algorithm>
#include <format>

using namespace WhiskerToolbox::Transforms::V2;
using namespace WhiskerToolbox::Transforms::V2::Examples;

// ============================================================================
// Construction / Destruction
// ============================================================================

PipelineStepListWidget::PipelineStepListWidget(QWidget * parent)
    : QWidget(parent) {

    auto * main_layout = new QVBoxLayout(this);
    main_layout->setContentsMargins(0, 0, 0, 0);
    main_layout->setSpacing(4);

    // --- Steps Table ---
    _steps_table = new QTableWidget(0, 4, this);
    _steps_table->setHorizontalHeaderLabels({tr("#"), tr("Input Type"), tr("Transform"), tr("Output Type")});
    _steps_table->setSelectionBehavior(QAbstractItemView::SelectRows);
    _steps_table->setSelectionMode(QAbstractItemView::SingleSelection);
    _steps_table->setEditTriggers(QAbstractItemView::NoEditTriggers);
    _steps_table->verticalHeader()->setVisible(false);
    _steps_table->setMinimumHeight(80);

    // Column sizing
    _steps_table->horizontalHeader()->setSectionResizeMode(0, QHeaderView::ResizeToContents);// #
    _steps_table->horizontalHeader()->setSectionResizeMode(1, QHeaderView::Stretch);         // Input Type
    _steps_table->horizontalHeader()->setSectionResizeMode(2, QHeaderView::Stretch);         // Transform
    _steps_table->horizontalHeader()->setSectionResizeMode(3, QHeaderView::Stretch);         // Output Type

    main_layout->addWidget(_steps_table, 1);

    // --- Button row (remove only) ---
    auto * button_layout = new QHBoxLayout();
    button_layout->setSpacing(4);

    _remove_button = new QPushButton(tr("Remove Step"), this);
    _remove_button->setToolTip(tr("Remove the selected pipeline step"));
    _remove_button->setEnabled(false);
    button_layout->addWidget(_remove_button);

    button_layout->addStretch();
    main_layout->addLayout(button_layout);

    // --- Available Transforms Browser ---
    _browser_label = new QLabel(tr("Available Transforms"), this);
    _browser_label->setStyleSheet("font-weight: bold; margin-top: 6px;");
    main_layout->addWidget(_browser_label);

    _browser_table = new QTableWidget(0, 3, this);
    _browser_table->setHorizontalHeaderLabels({tr("Name"), tr("Description"), tr("Output Type")});
    _browser_table->setSelectionBehavior(QAbstractItemView::SelectRows);
    _browser_table->setSelectionMode(QAbstractItemView::SingleSelection);
    _browser_table->setEditTriggers(QAbstractItemView::NoEditTriggers);
    _browser_table->verticalHeader()->setVisible(false);
    _browser_table->setMinimumHeight(100);
    _browser_table->setToolTip(tr("Double-click a transform to add it to the pipeline"));

    // Column sizing for browser
    _browser_table->horizontalHeader()->setSectionResizeMode(0, QHeaderView::ResizeToContents);// Name
    _browser_table->horizontalHeader()->setSectionResizeMode(1, QHeaderView::Stretch);         // Description
    _browser_table->horizontalHeader()->setSectionResizeMode(2, QHeaderView::ResizeToContents);// Output Type

    main_layout->addWidget(_browser_table, 2);

    // --- Connections ---
    connect(_remove_button, &QPushButton::clicked, this, &PipelineStepListWidget::onRemoveStepClicked);
    connect(_steps_table, &QTableWidget::currentCellChanged,
            this, [this](int /*row*/, int /*col*/, int /*prevRow*/, int /*prevCol*/) {
                onStepSelectionChanged();
            });
    connect(_browser_table, &QTableWidget::cellDoubleClicked,
            this, &PipelineStepListWidget::onAvailableTransformDoubleClicked);
}

PipelineStepListWidget::~PipelineStepListWidget() = default;

// ============================================================================
// Public API
// ============================================================================

void PipelineStepListWidget::setInputType(std::type_index element_type,
                                          std::type_index container_type) {
    _input_element_type = element_type;
    _input_container_type = container_type;
    refreshChain();
}

std::type_index PipelineStepListWidget::currentOutputElementType() const {
    return _chain_result.output_element_type;
}

std::type_index PipelineStepListWidget::currentOutputContainerType() const {
    return _chain_result.output_container_type;
}

int PipelineStepListWidget::selectedStepIndex() const {
    return _steps_table->currentRow();
}

void PipelineStepListWidget::clearSteps() {
    _steps.clear();
    _steps_table->setRowCount(0);
    refreshChain();
    emit pipelineChanged();
    emit stepSelected(-1);
}

bool PipelineStepListWidget::addStep(std::string const & transform_name,
                                     std::string const & params_json) {
    auto & registry = ElementRegistry::instance();

    PipelineStepEntry entry;
    entry.step_id = std::format("step_{}", _steps.size() + 1);
    entry.transform_name = transform_name;
    entry.parameters_json = params_json;
    entry.is_container_transform = registry.isContainerTransform(transform_name);

    // Look up metadata from either element or container registry
    auto const * meta = registry.getMetadata(transform_name);
    if (meta) {
        entry.input_type = meta->input_type;
        entry.output_type = meta->output_type;
    } else {
        auto const * cmeta = registry.getContainerMetadata(transform_name);
        if (!cmeta) {
            return false;
        }
        entry.input_type = cmeta->input_container_type;
        entry.output_type = cmeta->output_container_type;
    }

    _steps.push_back(std::move(entry));
    refreshChain();

    // Select the newly added step
    _steps_table->setCurrentCell(static_cast<int>(_steps.size()) - 1, 0);

    emit pipelineChanged();
    return true;
}

void PipelineStepListWidget::updateStepParams(int step_index, std::string const & params_json) {
    if (step_index < 0 || step_index >= static_cast<int>(_steps.size())) {
        return;
    }
    _steps[static_cast<size_t>(step_index)].parameters_json = params_json;
    emit pipelineChanged();
}

// ============================================================================
// Slots
// ============================================================================

void PipelineStepListWidget::onRemoveStepClicked() {
    int row = _steps_table->currentRow();
    if (row < 0 || row >= static_cast<int>(_steps.size())) {
        return;
    }

    _steps.erase(_steps.begin() + row);
    refreshChain();

    // Select the previous step or the first one
    if (!_steps.empty()) {
        int new_row = std::min(row, static_cast<int>(_steps.size()) - 1);
        _steps_table->setCurrentCell(new_row, 0);
    }

    emit pipelineChanged();
}

void PipelineStepListWidget::onStepSelectionChanged() {
    updateButtonStates();
    updateAvailableTransforms();
    emit stepSelected(_steps_table->currentRow());
}

void PipelineStepListWidget::onAvailableTransformDoubleClicked(int row, int /*column*/) {
    if (row < 0 || row >= static_cast<int>(_current_compatible.size())) {
        return;
    }

    auto const & transform_name = _current_compatible[static_cast<size_t>(row)];
    addStep(transform_name);
}

// ============================================================================
// Core chain resolution
// ============================================================================

void PipelineStepListWidget::refreshChain() {
    // Extract step names
    std::vector<std::string> step_names;
    step_names.reserve(_steps.size());
    for (auto const & s : _steps) {
        step_names.push_back(s.transform_name);
    }

    // Resolve the full type chain in one call
    _chain_result = resolveTypeChain(_input_container_type, step_names);

    // Propagate validity back to the step entries
    for (size_t i = 0; i < _steps.size() && i < _chain_result.steps.size(); ++i) {
        _steps[i].is_valid = _chain_result.steps[i].is_valid;
    }

    rebuildStepsTable();
    updateAvailableTransforms();
    emit validationChanged(_chain_result.all_valid);
}

// ============================================================================
// Table display
// ============================================================================

void PipelineStepListWidget::rebuildStepsTable() {
    int saved_row = _steps_table->currentRow();
    _steps_table->blockSignals(true);
    _steps_table->setRowCount(0);

    for (size_t i = 0; i < _steps.size() && i < _chain_result.steps.size(); ++i) {
        auto const & step = _steps[i];
        auto const & type_info = _chain_result.steps[i];
        int row = static_cast<int>(i);
        _steps_table->insertRow(row);

        // Column 0: Step number
        auto * num_item = new QTableWidgetItem(QString::number(i + 1));
        num_item->setTextAlignment(Qt::AlignCenter);
        _steps_table->setItem(row, 0, num_item);

        // Column 1: Input type
        _steps_table->setItem(row, 1,
                new QTableWidgetItem(QString::fromStdString(type_info.input_type_name)));

        // Column 2: Transform name
        _steps_table->setItem(row, 2,
                new QTableWidgetItem(QString::fromStdString(step.transform_name)));

        // Column 3: Output type
        _steps_table->setItem(row, 3,
                new QTableWidgetItem(QString::fromStdString(type_info.output_type_name)));

        // Highlight invalid steps
        if (!step.is_valid) {
            QColor invalid_bg(255, 200, 200);// Light red
            for (int col = 0; col < 4; ++col) {
                _steps_table->item(row, col)->setBackground(invalid_bg);
                _steps_table->item(row, col)->setToolTip(
                        tr("Type mismatch: this step's input type is incompatible "
                           "with the previous step's output type"));
            }
        }
    }

    // Restore selection
    if (saved_row >= 0 && saved_row < _steps_table->rowCount()) {
        _steps_table->setCurrentCell(saved_row, 0);
    }

    _steps_table->blockSignals(false);
    updateButtonStates();
}

void PipelineStepListWidget::updateAvailableTransforms() {
    // Determine the type context from the end of the resolved chain
    auto elem_type = _chain_result.output_element_type;
    auto cont_type = _chain_result.output_container_type;

    auto compatible = getCompatibleTransforms(elem_type, cont_type);
    _current_compatible = compatible;

    _browser_table->blockSignals(true);
    _browser_table->setRowCount(0);

    auto & registry = ElementRegistry::instance();

    for (size_t i = 0; i < compatible.size(); ++i) {
        auto const & name = compatible[i];
        int row = static_cast<int>(i);
        _browser_table->insertRow(row);

        // Column 0: Name
        _browser_table->setItem(row, 0, new QTableWidgetItem(QString::fromStdString(name)));

        // Column 1: Description
        std::string description;
        std::string output_type_name;

        auto const * meta = registry.getMetadata(name);
        if (meta) {
            description = meta->description;
        } else {
            auto const * cmeta = registry.getContainerMetadata(name);
            if (cmeta) {
                description = cmeta->description;
            }
        }
        _browser_table->setItem(row, 1, new QTableWidgetItem(QString::fromStdString(description)));

        // Column 2: Output type — resolve by probing a one-step chain
        auto probe = resolveTypeChain(cont_type, std::span(&name, 1));
        if (!probe.steps.empty()) {
            output_type_name = probe.steps[0].output_type_name;
        }
        _browser_table->setItem(row, 2, new QTableWidgetItem(QString::fromStdString(output_type_name)));
    }

    _browser_table->blockSignals(false);

    // Update label
    if (_input_element_type == typeid(void) && _input_container_type == typeid(void)) {
        _browser_label->setText(tr("Available Transforms (select input data first)"));
    } else if (compatible.empty()) {
        _browser_label->setText(tr("Available Transforms (none for current type)"));
    } else {
        _browser_label->setText(tr("Available Transforms — double-click to add"));
    }
}

std::vector<std::string> PipelineStepListWidget::getCompatibleTransforms(
        std::type_index element_type,
        std::type_index container_type) const {

    auto & registry = ElementRegistry::instance();

    // Gather both element-level and container-level transforms
    auto element_transforms = registry.getTransformsForInputType(element_type);
    auto container_transforms = registry.getContainerTransformsForInputType(container_type);

    // Merge and deduplicate
    std::vector<std::string> result;
    result.reserve(element_transforms.size() + container_transforms.size());
    result.insert(result.end(), element_transforms.begin(), element_transforms.end());
    result.insert(result.end(), container_transforms.begin(), container_transforms.end());

    // Filter out "WithContext" variants — they are internal duplicates
    // of the base transforms used only for context-aware pipeline execution.
    std::erase_if(result, [](std::string const & n) {
        return n.size() > 11 && n.ends_with("WithContext");
    });

    std::sort(result.begin(), result.end());
    result.erase(std::unique(result.begin(), result.end()), result.end());

    return result;
}

void PipelineStepListWidget::updateButtonStates() {
    int row = _steps_table->currentRow();
    bool has_selection = (row >= 0);
    _remove_button->setEnabled(has_selection);
}

// ============================================================================
// Load from descriptors
// ============================================================================

bool PipelineStepListWidget::loadFromDescriptors(
        std::vector<PipelineStepDescriptor> const & descriptors) {

    blockSignals(true);

    _steps.clear();
    _steps_table->setRowCount(0);

    bool all_ok = true;

    for (auto const & desc : descriptors) {
        // Serialize the step's parameters back to JSON for the params_json field
        std::string params_json = "{}";
        if (desc.parameters.has_value()) {
            params_json = rfl::json::write(desc.parameters.value());
        }

        auto & registry = ElementRegistry::instance();
        PipelineStepEntry entry;
        entry.step_id = desc.step_id;
        entry.transform_name = desc.transform_name;
        entry.parameters_json = params_json;
        entry.is_container_transform = registry.isContainerTransform(desc.transform_name);

        // Look up metadata from either element or container registry
        auto const * meta = registry.getMetadata(desc.transform_name);
        if (meta) {
            entry.input_type = meta->input_type;
            entry.output_type = meta->output_type;
        } else {
            auto const * cmeta = registry.getContainerMetadata(desc.transform_name);
            if (!cmeta) {
                all_ok = false;
                continue;
            }
            entry.input_type = cmeta->input_container_type;
            entry.output_type = cmeta->output_container_type;
        }

        _steps.push_back(std::move(entry));
    }

    blockSignals(false);

    refreshChain();

    // Select first step if any
    if (!_steps.empty()) {
        _steps_table->setCurrentCell(0, 0);
    }

    emit pipelineChanged();
    return all_ok;
}
