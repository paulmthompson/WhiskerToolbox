#ifndef OBSERVER_DATA_HPP
#define OBSERVER_DATA_HPP

#include <functional>
#include <vector>

class ObserverData {

public:
    ObserverData() = default;

    using ObserverCallback = std::function<void()>;


    void addObserver(ObserverCallback callback) {
        _observers.push_back(std::move(callback));
    }

    void notifyObservers() {
        for (auto& observer : _observers) {
            observer(); // Call the observer callback
        }
    }

private:
    std::vector<ObserverCallback> _observers;
};

#endif // OBSERVER_DATA_HPP
