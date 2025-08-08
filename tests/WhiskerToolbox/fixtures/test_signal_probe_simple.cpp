#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>

#include "signal_probe.hpp"
#include "simple_test_emitter.hpp"
#include "../fixtures/qt_test_fixtures.hpp"

#include <QObject>
#include <QTimer>

TEST_CASE_METHOD(QtWidgetTestFixture, "Fixture Test - SignalProbe - Simple test", "[signal_probe][simple]") {
    // Create test emitter
    SimpleTestEmitter emitter;
    
    // Create signal probe
    SignalProbe stringProbe;
    stringProbe.connectTo(&emitter, &SimpleTestEmitter::testSignal);
    
    // Initially, no signal should be triggered
    REQUIRE(!stringProbe.wasTriggered());
    REQUIRE(stringProbe.getCallCount() == 0);
    
    // Emit signal
    emitter.emitTestSignal("Hello, World!");
    
    // Process events to ensure signals are delivered
    processEvents();
    
    // Verify signal was triggered
    REQUIRE(stringProbe.wasTriggered());
    REQUIRE(stringProbe.getCallCount() == 1);
    REQUIRE(stringProbe.getLastArg() == "Hello, World!");
}

TEST_CASE_METHOD(QtWidgetTestFixture, "Fixture Test - SignalProbe - Multiple emissions simple", "[signal_probe][multiple_simple]") {
    // Create test emitter
    SimpleTestEmitter emitter;
    
    // Create signal probe
    SignalProbe stringProbe;
    stringProbe.connectTo(&emitter, &SimpleTestEmitter::testSignal);
    
    // Emit multiple signals
    emitter.emitTestSignal("First");
    emitter.emitTestSignal("Second");
    emitter.emitTestSignal("Third");
    
    // Process events
    processEvents();
    
    // Verify call count
    REQUIRE(stringProbe.getCallCount() == 3);
    
    // Verify last arguments
    REQUIRE(stringProbe.getLastArg() == "Third");
    
    // Verify all arguments
    auto allArgs = stringProbe.getAllArgs();
    REQUIRE(allArgs.size() == 3);
    REQUIRE(allArgs[0] == "First");
    REQUIRE(allArgs[1] == "Second");
    REQUIRE(allArgs[2] == "Third");
}

TEST_CASE_METHOD(QtWidgetTestFixture, "Fixture Test - SignalProbe - Reset functionality simple", "[signal_probe][reset_simple]") {
    // Create test emitter
    SimpleTestEmitter emitter;
    
    // Create signal probe
    SignalProbe stringProbe;
    stringProbe.connectTo(&emitter, &SimpleTestEmitter::testSignal);
    
    // Emit signal
    emitter.emitTestSignal("Test");
    processEvents();
    
    // Verify signal was triggered
    REQUIRE(stringProbe.wasTriggered());
    REQUIRE(stringProbe.getCallCount() == 1);
    
    // Reset the probe
    stringProbe.reset();
    
    // Verify reset worked
    REQUIRE(!stringProbe.wasTriggered());
    REQUIRE(stringProbe.getCallCount() == 0);
    REQUIRE(!stringProbe.getLastArgs().has_value());
    REQUIRE(stringProbe.getAllArgs().empty());
    
    // Emit another signal
    emitter.emitTestSignal("After Reset");
    processEvents();
    
    // Verify new signal was captured
    REQUIRE(stringProbe.wasTriggered());
    REQUIRE(stringProbe.getCallCount() == 1);
    REQUIRE(stringProbe.getLastArg() == "After Reset");
} 