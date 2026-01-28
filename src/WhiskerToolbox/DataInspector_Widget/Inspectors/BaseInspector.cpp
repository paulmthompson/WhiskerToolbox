#include "BaseInspector.hpp"

#include "DataManager/DataManager.hpp"

BaseInspector::BaseInspector(
    std::shared_ptr<DataManager> data_manager,
    GroupManager * group_manager,
    QWidget * parent)
    : QWidget(parent)
    , _data_manager(std::move(data_manager))
    , _group_manager(group_manager) {
}

BaseInspector::~BaseInspector() {
    // Subclasses should call removeCallbacks() in their destructors
    // before this base destructor runs
}

void BaseInspector::setGroupManager(GroupManager * group_manager) {
    _group_manager = group_manager;
}

void BaseInspector::removeCallbackFromData(std::string const & key, int & callback_id) {
    // This is a placeholder - each concrete inspector handles its own
    // callback removal through the wrapped widget's removeCallbacks() method.
    // This method is provided for consistency but is typically not used.
    (void)key;  // Unused
    callback_id = -1;
}
