#include "DataManagerParameter_Widget.hpp"

#include <QMetaObject>

// Prefer the same include form used elsewhere in WhiskerToolbox UI code
#include "DataManager.hpp"

void DataManagerParameter_Widget::setDataManager(std::shared_ptr<DataManager> dm) {
    if (_data_manager == dm) {
        return;
    }

    _data_manager = std::move(dm);

    if (_data_manager) {
        // Track which DataManager this connection belongs to so we can ignore stale callbacks
        _connected_dm = _data_manager.get();

        QPointer<DataManagerParameter_Widget> self(this);
        DataManager * expected_dm = _connected_dm;
        (void)expected_dm; // silence static analyzer about capture-only usage

        _data_manager->addObserver([self, expected_dm]() {
            if (!self) return; // widget deleted
            // Drop callbacks that belong to a previous DataManager after a switch
            if (self->_connected_dm != expected_dm) return;

            QMetaObject::invokeMethod(self, [self]() {
                if (!self) return;
                self->onDataManagerDataChanged();
            }, Qt::QueuedConnection);
        });
    } else {
        _connected_dm = nullptr;
    }

    // Notify derived widgets that the manager changed
    onDataManagerChanged();
}
