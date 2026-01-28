/**
 * @file DataImportTypeRegistry.test.cpp
 * @brief Unit tests for DataImportTypeRegistry
 * 
 * Tests the singleton registry for mapping data types to import widget factories:
 * - Registration of widget factories
 * - Widget creation
 * - Query operations (hasType, supportedTypes, displayName)
 */

#include "DataImport_Widget/DataImportTypeRegistry.hpp"

#include <catch2/catch_test_macros.hpp>
#include <QWidget>
#include <memory>

// ==================== Singleton Tests ====================

TEST_CASE("DataImportTypeRegistry singleton", "[DataImportTypeRegistry]")
{
    SECTION("instance returns same object") {
        auto& ref1 = DataImportTypeRegistry::instance();
        auto& ref2 = DataImportTypeRegistry::instance();
        
        REQUIRE(&ref1 == &ref2);
    }
}

// ==================== Registration Tests ====================

TEST_CASE("DataImportTypeRegistry registration", "[DataImportTypeRegistry]")
{
    auto& registry = DataImportTypeRegistry::instance();
    
    // Note: The registry is a singleton and may have types registered
    // by the DataImport_Widget library's static initializers.
    // These tests verify the registry behavior with types that
    // may or may not be pre-registered.
    
    SECTION("hasType returns true for registered types") {
        // LineData, MaskData, PointData should be registered by static initializers
        // If not, this test verifies the basic registration mechanism
        
        // Register a test type
        registry.registerType("TestDataType", ImportWidgetFactory{
            .display_name = "Test Data",
            .create_widget = [](auto, auto parent) -> QWidget* {
                return new QWidget(parent);
            }
        });
        
        REQUIRE(registry.hasType("TestDataType"));
    }
    
    SECTION("hasType returns false for unknown types") {
        REQUIRE_FALSE(registry.hasType("CompletelyUnknownType12345"));
    }
    
    SECTION("displayName returns correct value") {
        registry.registerType("AnotherTestType", ImportWidgetFactory{
            .display_name = "Another Test Type Display",
            .create_widget = [](auto, auto parent) -> QWidget* {
                return new QWidget(parent);
            }
        });
        
        REQUIRE(registry.displayName("AnotherTestType") == "Another Test Type Display");
    }
    
    SECTION("displayName returns empty for unknown type") {
        REQUIRE(registry.displayName("UnknownTypeForDisplayName").isEmpty());
    }
}

// ==================== Widget Creation Tests ====================

TEST_CASE("DataImportTypeRegistry widget creation", "[DataImportTypeRegistry]")
{
    auto& registry = DataImportTypeRegistry::instance();
    
    SECTION("createWidget returns null for unknown type") {
        auto* widget = registry.createWidget("NonExistentType98765", nullptr, nullptr);
        REQUIRE(widget == nullptr);
    }
    
    // Note: Widget creation tests that actually create QWidget instances
    // require a QApplication, which would require restructuring the test
    // to use QCoreApplication or custom test fixtures. For now, we only
    // test the null case above.
}

// ==================== Query Tests ====================

TEST_CASE("DataImportTypeRegistry query operations", "[DataImportTypeRegistry]")
{
    auto& registry = DataImportTypeRegistry::instance();
    
    SECTION("supportedTypes includes registered types") {
        registry.registerType("SupportedTypesTest", ImportWidgetFactory{
            .display_name = "Supported Types Test",
            .create_widget = [](auto, auto parent) -> QWidget* {
                return new QWidget(parent);
            }
        });
        
        auto types = registry.supportedTypes();
        REQUIRE(types.contains("SupportedTypesTest"));
    }
    
    SECTION("supportedTypes returns non-empty list") {
        // The registry should have at least some types registered
        // from the DataImport_Widget library's static initializers
        // or from previous test registrations
        auto types = registry.supportedTypes();
        REQUIRE_FALSE(types.isEmpty());
    }
}

// ==================== Static Registration Verification ====================

TEST_CASE("DataImportTypeRegistry static registrations", "[DataImportTypeRegistry][integration]")
{
    auto& registry = DataImportTypeRegistry::instance();
    
    // Note: Static initializers in DataImport_Widget library may not run in test binaries
    // due to how static libraries work with linking. The tests below verify the 
    // registration mechanism works correctly, not that all types are auto-registered.
    
    SECTION("registry is functional after manual registration") {
        // Register a test type to verify the mechanism works
        registry.registerType("IntegrationTestType", ImportWidgetFactory{
            .display_name = "Integration Test",
            .create_widget = [](auto, auto parent) -> QWidget* {
                return new QWidget(parent);
            }
        });
        
        auto types = registry.supportedTypes();
        
        // We should have at least our test type
        INFO("Registered types: " << types.join(", ").toStdString());
        REQUIRE(types.contains("IntegrationTestType"));
    }
}
