/**
 * @file KeyActionAdapter.hpp
 * @brief Composition-based component for receiving keymap dispatches
 *
 * Widgets create a KeyActionAdapter as a child QObject to receive
 * action dispatches from KeymapManager. This avoids multiple inheritance
 * — widgets don't need to implement any interface.
 *
 * The adapter's lifetime is tied to the widget via Qt parent-child ownership.
 * KeymapManager detects destruction via QObject::destroyed to clean up routing.
 *
 * @see KeymapManager::registerAdapter for registration
 * @see KeyAction.hpp for action descriptors
 */

#ifndef KEYMAP_SYSTEM_KEY_ACTION_ADAPTER_HPP
#define KEYMAP_SYSTEM_KEY_ACTION_ADAPTER_HPP

#include <QObject>
#include <QString>

#include "EditorState/StrongTypes.hpp"

#include <functional>

namespace KeymapSystem {

/**
 * @brief Component for receiving keymap dispatches via composition
 *
 * Usage:
 * @code
 * // In widget constructor:
 * _key_adapter = new KeyActionAdapter(this);  // QObject parenting
 * _key_adapter->setInstanceId(_state->getInstanceId());
 * _key_adapter->setHandler([this](QString const & action_id) -> bool {
 *     if (action_id == "media.assign_group_1") {
 *         assignToGroup(1);
 *         return true;
 *     }
 *     return false;
 * });
 * keymap_manager->registerAdapter(_key_adapter);
 * @endcode
 */
class KeyActionAdapter : public QObject {
    Q_OBJECT
public:
    /// Callback type: receives action_id, returns true if handled
    using Handler = std::function<bool(QString const &)>;

    explicit KeyActionAdapter(QObject * parent = nullptr);

    /// Set the handler called when an action is dispatched to this adapter
    void setHandler(Handler handler);

    /**
     * @brief Called by KeymapManager to dispatch an action
     * @param action_id The action to handle
     * @return true if the action was handled
     */
    bool handleKeyAction(QString const & action_id);

    /// Set the EditorInstanceId this adapter is associated with
    void setInstanceId(EditorLib::EditorInstanceId const & id);

    /// Get the EditorInstanceId this adapter is associated with
    [[nodiscard]] EditorLib::EditorInstanceId instanceId() const;

    /// Set the EditorTypeId this adapter is associated with
    void setTypeId(EditorLib::EditorTypeId const & id);

    /// Get the EditorTypeId this adapter is associated with
    [[nodiscard]] EditorLib::EditorTypeId typeId() const;

private:
    Handler _handler;
    EditorLib::EditorInstanceId _instance_id;
    EditorLib::EditorTypeId _type_id;
};

}// namespace KeymapSystem

#endif// KEYMAP_SYSTEM_KEY_ACTION_ADAPTER_HPP
