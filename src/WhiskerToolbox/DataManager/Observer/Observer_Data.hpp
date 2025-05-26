#ifndef OBSERVER_DATA_HPP
#define OBSERVER_DATA_HPP

#include <functional>
#include <unordered_map>

class ObserverData {

public:
    ObserverData() = default;

    using ObserverCallback = std::function<void()>;
    using CallbackID = int;

    CallbackID addObserver(ObserverCallback callback);

    void notifyObservers();

    void removeObserver(CallbackID id);

private:
    std::unordered_map<CallbackID, ObserverCallback> _observers;
};

#endif// OBSERVER_DATA_HPP
