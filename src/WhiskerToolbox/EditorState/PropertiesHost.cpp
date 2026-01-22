#include "PropertiesHost.hpp"

#include "EditorRegistry.hpp"
#include "EditorState.hpp"
#include "SelectionContext.hpp"

#include <QVBoxLayout>
#include <QLabel>

#include <iostream>

PropertiesHost::PropertiesHost(EditorRegistry * editor_registry, QWidget * parent)
    : QWidget(parent)
    , _editor_registry(editor_registry)
    , _selection_context(editor_registry ? editor_registry->selectionContext() : nullptr)
    , _stack(nullptr)
    , _placeholder(nullptr)
{
    setupUi();
    connectSignals();
}

PropertiesHost::~PropertiesHost() {
    // Cached widgets are children of _stack, so they'll be deleted automatically
    // Just clear the map
    _cached_widgets.clear();
}

QWidget * PropertiesHost::currentProperties() const {
    if (!_current_instance_id.isValid()) {
        return nullptr;
    }
    auto it = _cached_widgets.find(_current_instance_id);
    if (it != _cached_widgets.end()) {
        return it->second;
    }
    return nullptr;
}

EditorInstanceId PropertiesHost::currentInstanceId() const {
    return _current_instance_id;
}

void PropertiesHost::showPropertiesFor(EditorInstanceId const & instance_id) {
    if (!instance_id.isValid()) {
        showPlaceholder();
        return;
    }

    // Check if we already have this widget cached
    auto * props = getOrCreateProperties(instance_id);
    if (!props) {
        // Editor has no properties widget, show placeholder
        showPlaceholder();
        return;
    }

    // Switch to this widget
    _stack->setCurrentWidget(props);
    _current_instance_id = instance_id;
    
    emit propertiesChanged(instance_id);
}

void PropertiesHost::clearCachedProperties(EditorInstanceId const & instance_id) {
    auto it = _cached_widgets.find(instance_id);
    if (it != _cached_widgets.end()) {
        // If this is the current widget, switch to placeholder
        if (_current_instance_id == instance_id) {
            showPlaceholder();
        }
        
        // Remove from stack and delete
        _stack->removeWidget(it->second);
        delete it->second;
        _cached_widgets.erase(it);
    }
}

void PropertiesHost::clearAllCached() {
    showPlaceholder();
    
    for (auto & [id, widget] : _cached_widgets) {
        _stack->removeWidget(widget);
        delete widget;
    }
    _cached_widgets.clear();
}

void PropertiesHost::onActiveEditorChanged(EditorInstanceId const & instance_id) {
    showPropertiesFor(instance_id);
}

void PropertiesHost::onSelectionChanged(SelectionSource const & source) {
    // When selection changes, we might want to update properties
    // For now, we follow the active editor, which is updated separately
    // This slot is here for future enhancements (e.g., data-type specific property hints)
    Q_UNUSED(source)
}

void PropertiesHost::onEditorUnregistered(EditorInstanceId const & instance_id) {
    clearCachedProperties(instance_id);
}

void PropertiesHost::setupUi() {
    auto * layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);

    _stack = new QStackedWidget(this);
    layout->addWidget(_stack);

    // Create and add placeholder
    _placeholder = createPlaceholder();
    _stack->addWidget(_placeholder);
    _stack->setCurrentWidget(_placeholder);
}

void PropertiesHost::connectSignals() {
    if (!_editor_registry) {
        return;
    }

    // Connect to SelectionContext for active editor changes
    if (_selection_context) {
        connect(_selection_context, &SelectionContext::activeEditorChanged,
                this, &PropertiesHost::onActiveEditorChanged);
        connect(_selection_context, &SelectionContext::selectionChanged,
                this, &PropertiesHost::onSelectionChanged);
    }

    // Connect to EditorRegistry for editor lifecycle
    connect(_editor_registry, &EditorRegistry::stateUnregistered,
            this, &PropertiesHost::onEditorUnregistered);
}

QWidget * PropertiesHost::getOrCreateProperties(EditorInstanceId const & instance_id) {
    // Check cache first
    auto it = _cached_widgets.find(instance_id);
    if (it != _cached_widgets.end()) {
        return it->second;
    }

    // Get the state from registry
    auto state = _editor_registry->state(instance_id);
    if (!state) {
        std::cerr << "PropertiesHost: No state found for instance: " 
                  << instance_id.toStdString() << std::endl;
        return nullptr;
    }

    // Use EditorRegistry to create properties widget
    auto * props = _editor_registry->createProperties(state);
    if (!props) {
        // Editor type doesn't have a properties factory - that's okay
        return nullptr;
    }

    // Add to stack and cache
    _stack->addWidget(props);
    _cached_widgets[instance_id] = props;

    return props;
}

void PropertiesHost::showPlaceholder() {
    _stack->setCurrentWidget(_placeholder);
    EditorInstanceId old_id = _current_instance_id;
    _current_instance_id = EditorInstanceId{};
    
    if (old_id.isValid()) {
        emit propertiesChanged(EditorInstanceId{});
    }
}

QWidget * PropertiesHost::createPlaceholder() {
    auto * placeholder = new QWidget();
    auto * layout = new QVBoxLayout(placeholder);
    layout->setContentsMargins(16, 16, 16, 16);

    auto * title_label = new QLabel(tr("Properties"));
    title_label->setStyleSheet(QStringLiteral(
        "font-weight: bold; font-size: 14px; color: #666; margin-bottom: 8px;"
    ));
    title_label->setAlignment(Qt::AlignTop | Qt::AlignHCenter);

    auto * hint_label = new QLabel(tr("Select an editor to view its properties."));
    hint_label->setStyleSheet(QStringLiteral(
        "color: #888; font-style: italic;"
    ));
    hint_label->setAlignment(Qt::AlignTop | Qt::AlignHCenter);
    hint_label->setWordWrap(true);

    layout->addWidget(title_label);
    layout->addWidget(hint_label);
    layout->addStretch();

    return placeholder;
}
