#include "EditorFactory.hpp"

#include "EditorState.hpp"
#include "WorkspaceManager.hpp"

#include <iostream>

EditorFactory::EditorFactory(WorkspaceManager * workspace_manager,
                             std::shared_ptr<DataManager> data_manager,
                             QObject * parent)
    : QObject(parent)
    , _workspace_manager(workspace_manager)
    , _data_manager(std::move(data_manager)) {
}

bool EditorFactory::registerEditorType(EditorTypeInfo const & info,
                                       StateFactory state_factory,
                                       ViewFactory view_factory,
                                       PropertiesFactory properties_factory) {
    if (info.type_id.isEmpty()) {
        std::cerr << "EditorFactory::registerEditorType: type_id cannot be empty" << std::endl;
        return false;
    }

    if (!state_factory) {
        std::cerr << "EditorFactory::registerEditorType: state_factory cannot be null for "
                  << info.type_id.toStdString() << std::endl;
        return false;
    }

    if (!view_factory) {
        std::cerr << "EditorFactory::registerEditorType: view_factory cannot be null for "
                  << info.type_id.toStdString() << std::endl;
        return false;
    }

    if (_registrations.contains(info.type_id)) {
        std::cerr << "EditorFactory::registerEditorType: type_id already registered: "
                  << info.type_id.toStdString() << std::endl;
        return false;
    }

    _registrations[info.type_id] = EditorRegistration{
        .info = info,
        .state_factory = std::move(state_factory),
        .view_factory = std::move(view_factory),
        .properties_factory = std::move(properties_factory)};

    emit editorTypeRegistered(info.type_id);
    return true;
}

bool EditorFactory::unregisterEditorType(QString const & type_id) {
    auto it = _registrations.find(type_id);
    if (it == _registrations.end()) {
        return false;
    }

    _registrations.erase(it);
    emit editorTypeUnregistered(type_id);
    return true;
}

EditorFactory::EditorInstance EditorFactory::createEditor(QString const & type_id) {
    auto it = _registrations.find(type_id);
    if (it == _registrations.end()) {
        std::cerr << "EditorFactory::createEditor: unknown type_id: "
                  << type_id.toStdString() << std::endl;
        return EditorInstance{nullptr, nullptr, nullptr};
    }

    auto const & reg = it->second;

    // Create state
    auto state = reg.state_factory();
    if (!state) {
        std::cerr << "EditorFactory::createEditor: state_factory returned nullptr for "
                  << type_id.toStdString() << std::endl;
        return EditorInstance{nullptr, nullptr, nullptr};
    }

    // Register state with WorkspaceManager
    _workspace_manager->registerState(state);

    // Create view
    auto * view = reg.view_factory(state);
    if (!view) {
        std::cerr << "EditorFactory::createEditor: view_factory returned nullptr for "
                  << type_id.toStdString() << std::endl;
        // Unregister the state since we failed
        _workspace_manager->unregisterState(state->getInstanceId());
        return EditorInstance{nullptr, nullptr, nullptr};
    }

    // Create properties (optional)
    QWidget * properties = nullptr;
    if (reg.properties_factory) {
        properties = reg.properties_factory(state);
        // Properties can be nullptr without being an error
    }

    emit editorCreated(state->getInstanceId(), type_id);

    return EditorInstance{
        .state = state,
        .view = view,
        .properties = properties};
}

std::shared_ptr<EditorState> EditorFactory::createState(QString const & type_id) {
    auto it = _registrations.find(type_id);
    if (it == _registrations.end()) {
        std::cerr << "EditorFactory::createState: unknown type_id: "
                  << type_id.toStdString() << std::endl;
        return nullptr;
    }

    return it->second.state_factory();
}

QWidget * EditorFactory::createView(std::shared_ptr<EditorState> state) {
    if (!state) {
        return nullptr;
    }

    QString type_id = state->getTypeName();
    auto it = _registrations.find(type_id);
    if (it == _registrations.end()) {
        std::cerr << "EditorFactory::createView: unknown type_id: "
                  << type_id.toStdString() << std::endl;
        return nullptr;
    }

    return it->second.view_factory(state);
}

QWidget * EditorFactory::createProperties(std::shared_ptr<EditorState> state) {
    if (!state) {
        return nullptr;
    }

    QString type_id = state->getTypeName();
    auto it = _registrations.find(type_id);
    if (it == _registrations.end()) {
        std::cerr << "EditorFactory::createProperties: unknown type_id: "
                  << type_id.toStdString() << std::endl;
        return nullptr;
    }

    if (!it->second.properties_factory) {
        // No properties factory registered - this is valid
        return nullptr;
    }

    return it->second.properties_factory(state);
}

bool EditorFactory::hasEditorType(QString const & type_id) const {
    return _registrations.contains(type_id);
}

EditorFactory::EditorTypeInfo EditorFactory::getEditorInfo(QString const & type_id) const {
    auto it = _registrations.find(type_id);
    if (it == _registrations.end()) {
        return EditorTypeInfo{};
    }
    return it->second.info;
}

std::vector<EditorFactory::EditorTypeInfo> EditorFactory::availableEditors() const {
    std::vector<EditorTypeInfo> result;
    result.reserve(_registrations.size());

    for (auto const & [type_id, reg] : _registrations) {
        result.push_back(reg.info);
    }

    return result;
}

std::vector<EditorFactory::EditorTypeInfo>
EditorFactory::editorsByMenuPath(QString const & menu_path) const {
    std::vector<EditorTypeInfo> result;

    for (auto const & [type_id, reg] : _registrations) {
        if (reg.info.menu_path == menu_path) {
            result.push_back(reg.info);
        }
    }

    return result;
}
