
#include "Observer_Data.hpp"

ObserverData::CallbackID ObserverData::addObserver(ObserverCallback callback) {
    auto id = _next_id++;
    _observers[id] = std::move(callback);
    return id;
}

void ObserverData::notifyObservers() {
    // Copy the callback map so observers can safely add/remove during iteration.
    auto const snapshot = _observers;
    for (auto const & [id, observer]: snapshot) {
        observer();
    }
}

void ObserverData::removeObserver(CallbackID id) {
    _observers.erase(id);
}
