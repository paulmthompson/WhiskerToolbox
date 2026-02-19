#include "PipelineStepListWidget.hpp"

#include "TransformsV2/core/ElementRegistry.hpp"
#include "TransformsV2/core/PipelineLoader.hpp"
#include "TransformsV2/detail/ContainerTraits.hpp"

#include <QHBoxLayout>
#include <QInputDialog>
#include <QListWidget>
#include <QMessageBox>
#include <QPushButton>
#include <QVBoxLayout>

#include <algorithm>
#include <format>
#include <iostream>

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

    // --- List widget ---
    _list_widget = new QListWidget(this);
    _list_widget->setSelectionMode(QAbstractItemView::SingleSelection);
    _list_widget->setDragDropMode(QAbstractItemView::NoDragDrop);
    _list_widget->setMinimumHeight(100);
    main_layout->addWidget(_list_widget, 1);

    // --- Button row ---
    auto * button_layout = new QHBoxLayout();
    button_layout->setSpacing(4);

    _add_button = new QPushButton(tr("+"), this);
    _add_button->setToolTip(tr("Add a new pipeline step"));
    _add_button->setFixedWidth(32);
    button_layout->addWidget(_add_button);

    _remove_button = new QPushButton(tr("✕"), this);
    _remove_button->setToolTip(tr("Remove selected step"));
    _remove_button->setFixedWidth(32);
    _remove_button->setEnabled(false);
    button_layout->addWidget(_remove_button);

    _move_up_button = new QPushButton(tr("↑"), this);
    _move_up_button->setToolTip(tr("Move step up"));
    _move_up_button->setFixedWidth(32);
    _move_up_button->setEnabled(false);
    button_layout->addWidget(_move_up_button);

    _move_down_button = new QPushButton(tr("↓"), this);
    _move_down_button->setToolTip(tr("Move step down"));
    _move_down_button->setFixedWidth(32);
    _move_down_button->setEnabled(false);
    button_layout->addWidget(_move_down_button);

    button_layout->addStretch();
    main_layout->addLayout(button_layout);

    // --- Connections ---
    connect(_add_button, &QPushButton::clicked, this, &PipelineStepListWidget::onAddStepClicked);
    connect(_remove_button, &QPushButton::clicked, this, &PipelineStepListWidget::onRemoveStepClicked);
    connect(_move_up_button, &QPushButton::clicked, this, &PipelineStepListWidget::onMoveUpClicked);
    connect(_move_down_button, &QPushButton::clicked, this, &PipelineStepListWidget::onMoveDownClicked);
    connect(_list_widget, &QListWidget::currentRowChanged, this, &PipelineStepListWidget::onSelectionChanged);
}

PipelineStepListWidget::~PipelineStepListWidget() = default;

// ============================================================================
// Public API
// ============================================================================

void PipelineStepListWidget::setInputType(std::type_index element_type,
                                          std::type_index container_type) {
    _input_element_type = element_type;
    _input_container_type = container_type;

    std::cout << "[PipelineStepList] setInputType:"
              << " element='" << _input_element_type.name() << "'"
              << " container='" << _input_container_type.name() << "'"
              << std::endl;

    validateTypeChain();
    rebuildListDisplay();
}

std::type_index PipelineStepListWidget::currentOutputElementType() const {
    if (_steps.empty()) {
        return _input_element_type;
    }
    return _steps.back().output_type;
}

std::type_index PipelineStepListWidget::currentOutputContainerType() const {
    if (_steps.empty()) {
        return _input_container_type;
    }
    // Try to map the output element type to a container type
    try {
        return TypeIndexMapper::elementToContainer(_steps.back().output_type);
    } catch (...) {
        return _input_container_type;
    }
}

int PipelineStepListWidget::selectedStepIndex() const {
    return _list_widget->currentRow();
}

void PipelineStepListWidget::clearSteps() {
    _steps.clear();
    _list_widget->clear();
    updateButtonStates();
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
        // Fall back to container transform metadata
        auto const * cmeta = registry.getContainerMetadata(transform_name);
        if (!cmeta) {
            return false;
        }
        entry.input_type = cmeta->input_container_type;
        entry.output_type = cmeta->output_container_type;
    }

    _steps.push_back(std::move(entry));

    validateTypeChain();
    rebuildListDisplay();

    // Select the newly added step
    _list_widget->setCurrentRow(static_cast<int>(_steps.size()) - 1);

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

void PipelineStepListWidget::onAddStepClicked() {
    // Determine the current chain end type
    auto current_element_type = currentOutputElementType();
    auto current_container_type = currentOutputContainerType();

    auto compatible = getCompatibleTransforms(current_element_type, current_container_type);

    if (compatible.empty()) {
        QMessageBox::information(this, tr("No Compatible Transforms"),
                                 tr("No transforms are available for the current output type."));
        return;
    }

    // Build list of display items
    QStringList items;
    for (auto const & name: compatible) {
        auto const * meta = ElementRegistry::instance().getMetadata(name);
        if (meta && !meta->description.empty()) {
            items << QString::fromStdString(
                    std::format("{} — {}", name, meta->description));
        } else {
            items << QString::fromStdString(name);
        }
    }

    bool ok = false;
    auto selected = QInputDialog::getItem(this, tr("Add Pipeline Step"),
                                          tr("Select transform:"),
                                          items, 0, false, &ok);
    if (!ok || selected.isEmpty()) {
        return;
    }

    // Extract transform name (before the " — " separator)
    auto name_str = selected.toStdString();
    auto dash_pos = name_str.find(" — ");
    if (dash_pos != std::string::npos) {
        name_str = name_str.substr(0, dash_pos);
    }

    addStep(name_str);
}

void PipelineStepListWidget::onRemoveStepClicked() {
    int row = _list_widget->currentRow();
    if (row < 0 || row >= static_cast<int>(_steps.size())) {
        return;
    }

    _steps.erase(_steps.begin() + row);
    validateTypeChain();
    rebuildListDisplay();

    // Select the previous step or the first one
    if (!_steps.empty()) {
        int new_row = std::min(row, static_cast<int>(_steps.size()) - 1);
        _list_widget->setCurrentRow(new_row);
    }

    emit pipelineChanged();
}

void PipelineStepListWidget::onMoveUpClicked() {
    int row = _list_widget->currentRow();
    if (row <= 0 || row >= static_cast<int>(_steps.size())) {
        return;
    }

    std::swap(_steps[static_cast<size_t>(row)], _steps[static_cast<size_t>(row - 1)]);
    validateTypeChain();
    rebuildListDisplay();
    _list_widget->setCurrentRow(row - 1);
    emit pipelineChanged();
}

void PipelineStepListWidget::onMoveDownClicked() {
    int row = _list_widget->currentRow();
    if (row < 0 || row >= static_cast<int>(_steps.size()) - 1) {
        return;
    }

    std::swap(_steps[static_cast<size_t>(row)], _steps[static_cast<size_t>(row + 1)]);
    validateTypeChain();
    rebuildListDisplay();
    _list_widget->setCurrentRow(row + 1);
    emit pipelineChanged();
}

void PipelineStepListWidget::onSelectionChanged() {
    updateButtonStates();
    emit stepSelected(_list_widget->currentRow());
}

// ============================================================================
// Private Helpers
// ============================================================================

void PipelineStepListWidget::rebuildListDisplay() {
    int saved_row = _list_widget->currentRow();
    _list_widget->blockSignals(true);
    _list_widget->clear();

    for (size_t i = 0; i < _steps.size(); ++i) {
        auto const & step = _steps[i];
        auto display = QString::fromStdString(
                std::format("{}. {}", i + 1, step.transform_name));

        auto * item = new QListWidgetItem(display, _list_widget);

        if (!step.is_valid) {
            item->setBackground(QColor(255, 200, 200));// Light red for invalid
            item->setToolTip(tr("Type mismatch: this step's input type is incompatible "
                                "with the previous step's output type"));
        }
    }

    // Restore selection
    if (saved_row >= 0 && saved_row < _list_widget->count()) {
        _list_widget->setCurrentRow(saved_row);
    }

    _list_widget->blockSignals(false);
    updateButtonStates();
}

void PipelineStepListWidget::validateTypeChain() {
    if (_steps.empty()) {
        emit validationChanged(true);
        return;
    }

    bool all_valid = true;
    auto current_element_type = _input_element_type;
    auto current_container_type = _input_container_type;

    for (auto & step: _steps) {
        auto const & registry = ElementRegistry::instance();

        if (step.is_container_transform) {
            // Container transforms match on container type
            auto const * cmeta = registry.getContainerMetadata(step.transform_name);
            if (cmeta) {
                step.is_valid = (cmeta->input_container_type == current_container_type);
                if (step.is_valid) {
                    current_container_type = cmeta->output_container_type;
                    // Update element type from the new container type
                    try {
                        current_element_type = TypeIndexMapper::containerToElement(current_container_type);
                    } catch (...) {
                        // Keep previous element type if mapping fails
                    }
                }
            } else {
                step.is_valid = false;
            }
        } else {
            // Element transforms match on element type
            step.is_valid = (step.input_type == current_element_type);
            if (step.is_valid) {
                current_element_type = step.output_type;
                // Update container type from new element type
                try {
                    current_container_type = TypeIndexMapper::elementToContainer(current_element_type);
                } catch (...) {
                    // Keep previous container type if mapping fails
                }
            }
        }

        if (!step.is_valid) {
            all_valid = false;
        }
    }

    emit validationChanged(all_valid);
}

std::vector<std::string> PipelineStepListWidget::getCompatibleTransforms(
        std::type_index element_type,
        std::type_index container_type) const {

    auto & registry = ElementRegistry::instance();

    // Gather both element-level and container-level transforms
    auto element_transforms = registry.getTransformsForInputType(element_type);
    auto container_transforms = registry.getContainerTransformsForInputType(container_type);

    std::cout << "[PipelineStepList] getCompatibleTransforms:"
              << " element_type='" << element_type.name() << "'"
              << " container_type='" << container_type.name() << "'"
              << " element_transforms=" << element_transforms.size()
              << " container_transforms=" << container_transforms.size()
              << std::endl;

    // Merge and deduplicate
    std::vector<std::string> result;
    result.reserve(element_transforms.size() + container_transforms.size());
    result.insert(result.end(), element_transforms.begin(), element_transforms.end());
    result.insert(result.end(), container_transforms.begin(), container_transforms.end());

    std::sort(result.begin(), result.end());
    result.erase(std::unique(result.begin(), result.end()), result.end());

    return result;
}

void PipelineStepListWidget::updateButtonStates() {
    int row = _list_widget->currentRow();
    bool has_selection = (row >= 0);
    int count = static_cast<int>(_steps.size());

    _remove_button->setEnabled(has_selection);
    _move_up_button->setEnabled(has_selection && row > 0);
    _move_down_button->setEnabled(has_selection && row < count - 1);
}

// ============================================================================
// Phase 2: Load from descriptors
// ============================================================================

bool PipelineStepListWidget::loadFromDescriptors(
        std::vector<PipelineStepDescriptor> const & descriptors) {

    // Block signals during bulk load to avoid intermediate emits
    blockSignals(true);

    _steps.clear();
    _list_widget->clear();

    bool all_ok = true;

    for (auto const & desc: descriptors) {
        // Serialize the step's parameters back to JSON for the params_json field
        std::string params_json = "{}";
        if (desc.parameters.has_value()) {
            // Convert rfl::Generic parameters to JSON string
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

    validateTypeChain();
    rebuildListDisplay();

    blockSignals(false);

    // Select first step if any
    if (!_steps.empty()) {
        _list_widget->setCurrentRow(0);
    }

    emit pipelineChanged();
    return all_ok;
}
