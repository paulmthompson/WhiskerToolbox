#ifndef OBSERVER_DATA_HPP
#define OBSERVER_DATA_HPP

#include <functional>
#include <unordered_map>
#include <vector>

class ObserverData {

public:
    ObserverData() = default;

    using ObserverCallback = std::function<void()>;
    using CallbackID = int;


    CallbackID addObserver(ObserverCallback callback) {

        auto id = _observers.size() + 1;

        _observers[id] = callback;

        return id;
    }

    void notifyObservers() {
        for (auto& [id, observer] : _observers) {
            observer(); // Call the observer callback
        }
    }

    void removeObserver(CallbackID id)
    {
        _observers.erase(id);
    }

private:
    std::unordered_map<CallbackID, ObserverCallback> _observers;
};

#endif // OBSERVER_DATA_HPP
