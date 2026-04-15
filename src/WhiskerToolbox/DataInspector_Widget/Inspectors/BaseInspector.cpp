#include "BaseInspector.hpp"

#include <utility>

#include "DataInspector_Widget/DataInspectorState.hpp"
#include "DataManager/DataManager.hpp"

BaseInspector::BaseInspector(
        std::shared_ptr<DataManager> data_manager,
        GroupManager * group_manager,
        QWidget * parent)
    : QWidget(parent),
      _data_manager(std::move(data_manager)),
      _group_manager(group_manager) {
}

BaseInspector::~BaseInspector() {
    // Subclasses should call removeCallbacks() in their destructors
    // before this base destructor runs
}

void BaseInspector::setGroupManager(GroupManager * group_manager) {
    _group_manager = group_manager;
}

void BaseInspector::setState(std::shared_ptr<DataInspectorState> state) {
    _state = std::move(state);
}

void BaseInspector::removeCallbackFromData(std::string const & key, int & callback_id) {
    if (!key.empty() && callback_id != -1 && _data_manager) {
        _data_manager->removeCallbackFromData(key, callback_id);
    }
    callback_id = -1;
}
