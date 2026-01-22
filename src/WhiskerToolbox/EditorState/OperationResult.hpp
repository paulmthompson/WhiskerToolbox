#ifndef OPERATION_RESULT_HPP
#define OPERATION_RESULT_HPP

/**
 * @file OperationResult.hpp
 * @brief Type-erased result container for inter-widget operations
 *
 * OperationResult wraps std::any with a DataChannel hint, allowing
 * widgets to pass arbitrary data through the OperationContext while
 * maintaining some type information for the receiver.
 *
 * ## Usage
 *
 * ```cpp
 * // Producer creates result
 * OperationResult result(DataChannels::TransformPipeline, my_pipeline);
 *
 * // Consumer extracts with type checking
 * if (auto* pipeline = result.peek<TransformPipeline>()) {
 *     use(*pipeline);
 * }
 * // Or take ownership (moves out of any)
 * auto pipeline = result.take<TransformPipeline>(); // throws if wrong type
 * ```
 *
 * @see OperationContext for operation management
 * @see DataChannel for channel identifiers
 */

#include "StrongTypes.hpp"

#include <any>
#include <optional>
#include <stdexcept>
#include <string>
#include <typeinfo>
#include <utility>

namespace EditorLib {

/**
 * @brief Exception thrown when OperationResult type extraction fails
 */
class OperationResultError : public std::runtime_error {
public:
    explicit OperationResultError(std::string const & msg)
        : std::runtime_error(msg) {}
};

/**
 * @brief Result payload from an operation
 *
 * Copyable container for operation results. Uses std::any internally
 * for type flexibility. The DataChannel indicates what type to expect.
 *
 * ## Thread Safety
 *
 * OperationResult is not thread-safe. All operations should occur
 * on the Qt main thread.
 */
class OperationResult {
public:
    /// Default constructor creates empty result
    OperationResult() = default;

    /// Construct with channel and payload
    template<typename T>
    OperationResult(DataChannel channel, T && payload)
        : _channel(std::move(channel))
        , _payload(std::forward<T>(payload)) {}

    // Copyable and movable (for Qt signal compatibility)
    OperationResult(OperationResult const &) = default;
    OperationResult & operator=(OperationResult const &) = default;
    OperationResult(OperationResult &&) = default;
    OperationResult & operator=(OperationResult &&) = default;

    /// Factory method with explicit channel
    template<typename T>
    static OperationResult create(DataChannel channel, T && value) {
        return OperationResult(std::move(channel), std::forward<T>(value));
    }

    /// Get the data channel
    [[nodiscard]] DataChannel const & channel() const { return _channel; }

    /// Check if payload has a value
    [[nodiscard]] bool hasPayload() const { return _payload.has_value(); }

    /// Check if this result is valid (has channel and payload)
    [[nodiscard]] bool isValid() const {
        return _channel.isValid() && _payload.has_value();
    }

    /**
     * @brief Extract payload as specific type (consuming)
     *
     * After calling this, the payload is moved out and hasPayload()
     * will return false.
     *
     * @tparam T Expected type
     * @return The value
     * @throws OperationResultError if payload is empty or type mismatch
     */
    template<typename T>
    T take() {
        if (!_payload.has_value()) {
            throw OperationResultError("Cannot take from empty payload");
        }

        try {
            T result = std::any_cast<T>(std::move(_payload));
            _payload.reset();
            return result;
        } catch (std::bad_any_cast const &) {
            throw OperationResultError(
                std::string("Type mismatch, expected: ") + typeid(T).name());
        }
    }

    /**
     * @brief Try to extract payload as specific type
     *
     * @tparam T Expected type
     * @return The value if type matches, or nullopt
     */
    template<typename T>
    [[nodiscard]] std::optional<T> tryTake() {
        if (!_payload.has_value()) {
            return std::nullopt;
        }

        try {
            T result = std::any_cast<T>(std::move(_payload));
            _payload.reset();
            return result;
        } catch (std::bad_any_cast const &) {
            return std::nullopt;
        }
    }

    /**
     * @brief Peek at payload without consuming
     *
     * @tparam T Expected type
     * @return Pointer to value if type matches, nullptr otherwise
     */
    template<typename T>
    [[nodiscard]] T const * peek() const {
        return std::any_cast<T>(&_payload);
    }

    /**
     * @brief Get a copy of the payload
     *
     * @tparam T Expected type
     * @return Copy of value if type matches, or nullopt
     */
    template<typename T>
    [[nodiscard]] std::optional<T> get() const {
        if (!_payload.has_value()) {
            return std::nullopt;
        }

        try {
            return std::any_cast<T>(_payload);
        } catch (std::bad_any_cast const &) {
            return std::nullopt;
        }
    }

private:
    DataChannel _channel;
    std::any _payload;
};

}  // namespace EditorLib

#endif  // OPERATION_RESULT_HPP
