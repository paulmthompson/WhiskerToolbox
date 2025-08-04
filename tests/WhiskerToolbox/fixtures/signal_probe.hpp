#ifndef SIGNAL_PROBE_HPP
#define SIGNAL_PROBE_HPP

#include <QObject>
#include <tuple>
#include <optional>
#include <vector>

/**
 * @brief A signal probe for testing Qt signals with QString arguments
 * 
 * This class provides a convenient way to monitor and test Qt signals
 * that emit QString arguments. It can capture signal arguments and track call counts.
 * 
 * @example
 * @code
 * // For a signal like: void mySignal(QString message);
 * SignalProbe probe;
 * probe.connectTo(widget, &MyWidget::mySignal);
 * 
 * // Trigger the signal
 * widget->emitMySignal("test");
 * 
 * // Check results
 * REQUIRE(probe.wasTriggered());
 * REQUIRE(probe.callCount == 1);
 * REQUIRE(probe.lastArgs.value() == "test");
 * @endcode
 */
class SignalProbe : public QObject {
    Q_OBJECT

public:
    /**
     * @brief Number of times the signal was emitted
     */
    int callCount = 0;

    /**
     * @brief Arguments from the last signal emission
     */
    std::optional<QString> lastArgs;

    /**
     * @brief Arguments from all signal emissions (in order)
     */
    std::vector<QString> allArgs;

    /**
     * @brief Connects the probe to a signal
     * 
     * @param sender The object emitting the signal
     * @param signal The signal to connect to
     */
    template <typename SenderType>
    void connectTo(const SenderType* sender, void (SenderType::*signal)(QString const &)) {
        QObject::connect(sender, signal, this, &SignalProbe::onSignal);
    }

    template <typename SenderType>
    void connectTo(const SenderType* sender, void (SenderType::*signal)(QString)) {
        QObject::connect(sender, signal, this, &SignalProbe::onSignal);
    }

    /**
     * @brief Resets the probe state
     * 
     * Clears call count and argument history
     */
    void reset() {
        callCount = 0;
        lastArgs.reset();
        allArgs.clear();
    }

    /**
     * @brief Checks if the signal was triggered at least once
     * 
     * @return True if the signal was emitted at least once
     */
    bool wasTriggered() const {
        return callCount > 0;
    }

    /**
     * @brief Gets the number of times the signal was emitted
     * 
     * @return The call count
     */
    int getCallCount() const {
        return callCount;
    }

    /**
     * @brief Gets the arguments from the last signal emission
     * 
     * @return Optional containing the last arguments, or empty if never triggered
     */
    const std::optional<QString>& getLastArgs() const {
        return lastArgs;
    }

    /**
     * @brief Gets all arguments from all signal emissions
     * 
     * @return Vector of all argument strings in order of emission
     */
    const std::vector<QString>& getAllArgs() const {
        return allArgs;
    }

    /**
     * @brief Gets the last argument value
     * 
     * @return The last argument value
     * @throws std::out_of_range if the signal was never triggered
     */
    QString getLastArg() const {
        if (!lastArgs.has_value()) {
            throw std::out_of_range("Signal was never triggered");
        }
        return lastArgs.value();
    }

    /**
     * @brief Gets a specific argument from a specific signal emission
     * 
     * @param emissionIndex The index of the signal emission (0-based)
     * @return The argument value
     * @throws std::out_of_range if emission index is invalid
     */
    QString getArg(size_t emissionIndex) const {
        if (emissionIndex >= allArgs.size()) {
            throw std::out_of_range("Emission index out of range");
        }
        return allArgs[emissionIndex];
    }

public slots:
    /**
     * @brief Slot that receives the signal
     * 
     * This slot is automatically connected to the signal and captures
     * the QString argument for later inspection in tests.
     * 
     * @param arg The signal argument
     */
    void onSignal(QString arg) {
        callCount++;
        lastArgs.emplace(arg);
        allArgs.push_back(arg);
    }
};

#endif // SIGNAL_PROBE_HPP 