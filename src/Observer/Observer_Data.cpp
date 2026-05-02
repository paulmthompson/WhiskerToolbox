
#include "Observer_Data.hpp"

#include <spdlog/spdlog.h>

#include <chrono>

ObserverData::CallbackID ObserverData::addObserver(ObserverCallback callback, std::string name) {
    auto id = _next_id++;
    _observers[id] = {std::move(callback), std::move(name)};
    return id;
}

void ObserverData::notifyObservers() {
    // Copy the callback map so observers can safely add/remove during iteration.
    auto const snapshot = _observers;

    // Log the domain name
    spdlog::debug("[{}] Initiating notification for {} observers", _name, snapshot.size());
    
    auto total_sw = std::chrono::steady_clock::now();

    for (auto const & [id, observer]: snapshot) {
        auto sw = std::chrono::steady_clock::now();
        observer.callback();
        spdlog::debug("[{}] Callback '{}' [{}] took {}ms", _name, observer.name, id, std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - sw).count());
    }

    spdlog::debug("[{}] Finished notifying {} observers. Total time: {}ms", 
        _name, snapshot.size(), std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - total_sw).count());
}

void ObserverData::removeObserver(CallbackID id) {
    _observers.erase(id);
}
