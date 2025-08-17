#include "IO/LoaderRegistry.hpp"
#include "IO/PluginLoader.hpp"
#include <iostream>

/**
 * @brief Demonstrate the plugin system functionality
 * 
 * This function can be called from DataManager constructor or tests
 * to verify that plugins are properly registered and working.
 */
void demonstratePluginSystem() {
    std::cout << "\n=== DataManager Plugin System Demo ===" << std::endl;
    
    auto& registry = LoaderRegistry::getInstance();
    auto formats = registry.getRegisteredFormats();
    
    std::cout << "Registered formats: ";
    for (auto const& format : formats) {
        std::cout << format << " ";
    }
    std::cout << std::endl;
    
    // Test specific format support
    if (PluginLoader::isFormatSupported("capnp", DM_DataType::Line)) {
        std::cout << "✓ CapnProto LineData loading is supported" << std::endl;
    } else {
        std::cout << "✗ CapnProto LineData loading is NOT supported" << std::endl;
    }
    
    std::cout << "=== End Plugin System Demo ===\n" << std::endl;
}
