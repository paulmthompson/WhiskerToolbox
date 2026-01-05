
#include "Observer_Data.hpp"

ObserverData::CallbackID ObserverData::addObserver(ObserverCallback callback) {

    auto id = static_cast<int>(_observers.size() + 1);

    _observers[id] = std::move(callback);

    return id;
}

void ObserverData::notifyObservers() {
    for (auto & [id, observer]: _observers) {
        observer();// Call the observer callback
    }
}

void ObserverData::removeObserver(CallbackID id) {
    _observers.erase(id);
}
