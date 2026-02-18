#include "PreReductionPanel.hpp"

#include "TransformsV2/core/PipelineLoader.hpp"
#include "TransformsV2/core/RangeReductionRegistry.hpp"

#include <QGroupBox>
#include <QHBoxLayout>
#include <QInputDialog>
#include <QLineEdit>
#include <QListWidget>
#include <QMessageBox>
#include <QPushButton>
#include <QVBoxLayout>

#include <algorithm>
#include <format>

using namespace WhiskerToolbox::Transforms::V2;

// ============================================================================
// Construction / Destruction
// ============================================================================

PreReductionPanel::PreReductionPanel(QWidget * parent)
    : QWidget(parent) {

    auto * main_layout = new QVBoxLayout(this);
    main_layout->setContentsMargins(0, 0, 0, 0);
    main_layout->setSpacing(0);

    // Collapsible group box
    _group_box = new QGroupBox(tr("Pre-Computations"), this);
    _group_box->setCheckable(true);
    _group_box->setChecked(false);
    main_layout->addWidget(_group_box);

    auto * group_layout = new QVBoxLayout(_group_box);
    group_layout->setSpacing(4);

    // List of pre-reductions
    _list_widget = new QListWidget(_group_box);
    _list_widget->setSelectionMode(QAbstractItemView::SingleSelection);
    _list_widget->setMaximumHeight(100);
    group_layout->addWidget(_list_widget);

    // Button row
    auto * button_layout = new QHBoxLayout();
    button_layout->setSpacing(4);

    _add_button = new QPushButton(tr("+"), _group_box);
    _add_button->setToolTip(tr("Add a pre-computation step"));
    _add_button->setFixedWidth(32);
    button_layout->addWidget(_add_button);

    _remove_button = new QPushButton(tr("✕"), _group_box);
    _remove_button->setToolTip(tr("Remove selected pre-computation"));
    _remove_button->setFixedWidth(32);
    _remove_button->setEnabled(false);
    button_layout->addWidget(_remove_button);

    button_layout->addStretch();
    group_layout->addLayout(button_layout);

    // Connections
    connect(_add_button, &QPushButton::clicked, this, &PreReductionPanel::onAddClicked);
    connect(_remove_button, &QPushButton::clicked, this, &PreReductionPanel::onRemoveClicked);
    connect(_list_widget, &QListWidget::currentRowChanged, this, [this](int row) {
        _remove_button->setEnabled(row >= 0);
    });
}

PreReductionPanel::~PreReductionPanel() = default;

// ============================================================================
// Public API
// ============================================================================

void PreReductionPanel::setInputType(std::type_index element_type) {
    _input_element_type = element_type;
}

void PreReductionPanel::clearEntries() {
    _entries.clear();
    _list_widget->clear();
    _remove_button->setEnabled(false);
    emit preReductionsChanged();
}

bool PreReductionPanel::addEntry(std::string const & reduction_name,
                                  std::string const & output_key) {
    PreReductionEntry entry;
    entry.reduction_name = reduction_name;
    entry.output_key = output_key;
    entry.parameters_json = "{}";

    _entries.push_back(std::move(entry));
    rebuildListDisplay();
    emit preReductionsChanged();
    return true;
}

// ============================================================================
// Slots
// ============================================================================

void PreReductionPanel::onAddClicked() {
    auto available = getAvailableReductions();

    if (available.empty()) {
        QMessageBox::information(this, tr("No Reductions Available"),
                                 tr("No pre-computation reductions are available for the current input type."));
        return;
    }

    QStringList items;
    for (auto const & name : available) {
        items << QString::fromStdString(name);
    }

    bool ok = false;
    auto selected = QInputDialog::getItem(this, tr("Add Pre-Computation"),
                                           tr("Select reduction:"),
                                           items, 0, false, &ok);
    if (!ok || selected.isEmpty()) {
        return;
    }

    auto reduction_name = selected.toStdString();

    // Ask for output key
    auto default_key = QString::fromStdString(reduction_name + "_result");
    auto output_key = QInputDialog::getText(this, tr("Output Key"),
                                             tr("Key name for this pre-computation:"),
                                             QLineEdit::Normal, default_key, &ok);
    if (!ok || output_key.isEmpty()) {
        return;
    }

    addEntry(reduction_name, output_key.toStdString());
}

void PreReductionPanel::onRemoveClicked() {
    int row = _list_widget->currentRow();
    if (row < 0 || row >= static_cast<int>(_entries.size())) {
        return;
    }

    _entries.erase(_entries.begin() + row);
    rebuildListDisplay();
    emit preReductionsChanged();
}

// ============================================================================
// Private Helpers
// ============================================================================

void PreReductionPanel::rebuildListDisplay() {
    _list_widget->clear();

    for (auto const & entry : _entries) {
        auto display = QString::fromStdString(
            std::format("{} → {}", entry.reduction_name, entry.output_key));
        _list_widget->addItem(display);
    }

    _remove_button->setEnabled(_list_widget->currentRow() >= 0);
}

std::vector<std::string> PreReductionPanel::getAvailableReductions() const {
    if (_input_element_type == typeid(void)) {
        return {};
    }

    auto & registry = RangeReductionRegistry::instance();
    return registry.getReductionsForInputType(_input_element_type);
}

// ============================================================================
// Phase 2: Load from descriptors
// ============================================================================

bool PreReductionPanel::loadFromDescriptors(
        std::vector<WhiskerToolbox::Transforms::V2::Examples::PreReductionStepDescriptor> const & descriptors) {

    _entries.clear();
    _list_widget->clear();

    bool all_ok = true;

    for (auto const & desc : descriptors) {
        PreReductionEntry entry;
        entry.reduction_name = desc.reduction_name;
        entry.output_key = desc.output_key;

        if (desc.parameters.has_value()) {
            entry.parameters_json = rfl::json::write(desc.parameters.value());
        } else {
            entry.parameters_json = "{}";
        }

        _entries.push_back(std::move(entry));
    }

    rebuildListDisplay();

    // Expand the group if entries were loaded
    if (!_entries.empty()) {
        _group_box->setChecked(true);
    }

    emit preReductionsChanged();
    return all_ok;
}
