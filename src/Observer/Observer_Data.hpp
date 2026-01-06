#ifndef OBSERVER_DATA_HPP
#define OBSERVER_DATA_HPP

/**
 * @file Observer_Data.hpp
 * @brief Observer pattern implementation for data change notifications
 *
 * This file provides the core observer pattern infrastructure used throughout
 * WhiskerToolbox for propagating data changes to dependent components.
 *
 * @note This library is NOT thread-safe. All observer operations (add, remove,
 *       notify) must be performed from the same thread, or external
 *       synchronization must be provided by the caller.
 */

#include <functional>
#include <unordered_map>

/**
 * @brief Strong type for specifying observer notification behavior
 *
 * This enum class makes the intent explicit when calling methods that modify
 * observer data. Users must explicitly choose whether to notify observers.
 *
 * This enum is designed for use by consuming libraries (e.g., DataManager)
 * to provide a clear API for methods that optionally notify observers:
 *
 * @code
 * void setData(const Data& data, NotifyObservers notify = NotifyObservers::Yes);
 * @endcode
 *
 * @see ObserverData for the observer registration and notification mechanism
 */
enum class NotifyObservers {
    Yes,  ///< Notify observers after the operation
    No    ///< Do not notify observers after the operation
};

/**
 * @brief Manages observer callbacks for implementing the observer pattern
 *
 * ObserverData provides a simple observer pattern implementation where
 * callbacks can be registered, removed, and notified of changes. Each
 * registered callback receives a unique ID that can be used for removal.
 *
 * @warning This class is NOT thread-safe. Concurrent calls to addObserver(),
 *          removeObserver(), or notifyObservers() from multiple threads
 *          will result in undefined behavior. Callers must provide external
 *          synchronization if thread safety is required.
 *
 * @warning Modifying observers (adding or removing) from within an observer
 *          callback during notifyObservers() results in undefined behavior.
 *
 * Example usage:
 * @code
 * ObserverData observable;
 *
 * // Register an observer
 * auto id = observable.addObserver([]() {
 *     std::cout << "Data changed!" << std::endl;
 * });
 *
 * // Trigger notifications
 * observable.notifyObservers();
 *
 * // Unregister when done
 * observable.removeObserver(id);
 * @endcode
 */
class ObserverData {

public:
    ObserverData() = default;

    /// @brief Type alias for observer callback functions
    using ObserverCallback = std::function<void()>;

    /// @brief Type alias for callback identifiers
    using CallbackID = int;

    /**
     * @brief Register a new observer callback
     *
     * Adds the given callback to the list of observers that will be notified
     * when notifyObservers() is called. The callback is stored by value.
     *
     * @param callback The callback function to invoke on notifications.
     *                 Must be a valid callable (non-empty std::function).
     * @return A unique identifier for this observer registration. Use this ID
     *         to remove the observer later via removeObserver().
     *
     * @note The returned ID should be stored if you intend to remove the
     *       observer later. Discarding the ID makes removal impossible.
     */
    [[nodiscard]] CallbackID addObserver(ObserverCallback callback);

    /**
     * @brief Notify all registered observers
     *
     * Invokes all registered observer callbacks. The order in which observers
     * are called is unspecified (depends on unordered_map iteration order).
     *
     * @note If any callback throws an exception, remaining callbacks will NOT
     *       be notified and the exception will propagate to the caller.
     */
    void notifyObservers();

    /**
     * @brief Remove a previously registered observer
     *
     * Removes the observer associated with the given ID. After removal, the
     * callback will no longer be invoked during notifyObservers().
     *
     * @param id The callback ID returned by addObserver(). If the ID is
     *           invalid or was already removed, this function does nothing.
     */
    void removeObserver(CallbackID id);

private:
    std::unordered_map<CallbackID, ObserverCallback> _observers;
    CallbackID _next_id = 1;  ///< Monotonically increasing ID counter
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
