#ifndef TABLE_OBSERVER_BRIDGE_HPP
#define TABLE_OBSERVER_BRIDGE_HPP

#include "utils/TableView/TableEvents.hpp"

class DataManager;

// Bridge function used by TableRegistry to notify DataManager observers
void DataManager__NotifyTableObservers(DataManager & dm, TableEvent const & ev);

#endif // TABLE_OBSERVER_BRIDGE_HPP

