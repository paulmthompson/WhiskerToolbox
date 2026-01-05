#ifndef OBSERVER_DATA_HPP
#define OBSERVER_DATA_HPP

#include <functional>
#include <optional>
#include <unordered_map>

/**
 * @brief Strong type for specifying observer notification behavior
 *
 * This enum class makes the intent explicit when calling methods that modify
 * observer data. Users must explicitly choose whether to notify observers.
 */
enum class NotifyObservers {
    Yes,  ///< Notify observers after the operation
    No    ///< Do not notify observers after the operation
};

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


/**
 * @brief A RAII handle for modifying an object and notifying on completion.
 *
 * This proxy object holds a reference to data (T&) and a callback.
 * When this handle is destroyed (goes out of scope), the callback is
 * automatically invoked, typically to notify observers of the change.
 *
 * It is not copyable, but it is movable.
 */
template <typename T>
class ModificationHandle {
public:
    // Constructor: Takes the data to modify and the function to call on destruction
    ModificationHandle(T& data_ref, std::function<void()> on_destroy_callback)
        : _data_ref(data_ref),
          _on_destroy_cb(std::move(on_destroy_callback)) {}

    // Destructor: The core RAII magic. This triggers the notification.
    ~ModificationHandle() {
        if (_on_destroy_cb) {
            _on_destroy_cb();
        }
    }

    // --- Accessors ---
    // Provide pointer-like access
    T* operator->() { return &_data_ref.get(); }
    T const* operator->() const { return &_data_ref.get(); }

    // Provide reference-like access
    T& operator*() { return _data_ref.get(); }
    T const& operator*() const { return _data_ref.get(); }

    // Explicit getter
    T& get() { return _data_ref.get(); }
    T const& get() const { return _data_ref.get(); }

    // --- Move Semantics ---
    // Enable moving: transfer ownership of the notification
    ModificationHandle(ModificationHandle&& other) noexcept
        : _data_ref(other._data_ref),
          _on_destroy_cb(std::move(other._on_destroy_cb)) {
        // Null out the other's callback so it doesn't fire
        other._on_destroy_cb = nullptr;
    }

    ModificationHandle& operator=(ModificationHandle&& other) noexcept {
        if (this != &other) {
            // If this handle is about to be overwritten, fire its own callback first
            // if it's still valid (or just let it be destroyed, depends on design)
            if (_on_destroy_cb) {
                _on_destroy_cb();
            }

            _data_ref = other._data_ref; // Note: std::reference_wrapper assignment re-seats the reference
            _on_destroy_cb = std::move(other._on_destroy_cb);
            other._on_destroy_cb = nullptr;
        }
        return *this;
    }

    // --- Disable Copying ---
    ModificationHandle(const ModificationHandle&) = delete;
    ModificationHandle& operator=(const ModificationHandle&) = delete;

private:
    std::reference_wrapper<T> _data_ref;
    std::function<void()> _on_destroy_cb;
};

#endif // OBSERVER_DATA_HPP
