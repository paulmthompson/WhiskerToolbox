/**
 * @file fuzz_example_template.cpp
 * @brief Template for creating new fuzz tests
 * 
 * This file serves as a template/example for adding new fuzz tests to the project.
 * Copy this file and modify it for your component.
 */

#include "fuzztest/fuzztest.h"
#include "gtest/gtest.h"

// Include your component's headers here
// #include "YourComponent/YourClass.hpp"

namespace {

// ============================================================================
// Example 1: Basic Fuzz Test
// ============================================================================

/**
 * @brief Simple fuzz test that accepts arbitrary strings
 * 
 * This test verifies that your function doesn't crash with any input.
 */
void FuzzBasicStringInput(const std::string& input) {
    try {
        // Call your function with the fuzzed input
        // YourFunction(input);
        
        // Add assertions if you want to check specific properties
        // EXPECT_TRUE(some_condition);
        
    } catch (const std::exception& e) {
        // Exceptions are generally acceptable for invalid input
        // The test passes as long as there's no crash
        // You can add logging if needed: std::cerr << "Exception: " << e.what() << std::endl;
    }
}
// Register the fuzz test
FUZZ_TEST(YourComponentFuzz, FuzzBasicStringInput);

// ============================================================================
// Example 2: Fuzz Test with Constrained Domains
// ============================================================================

/**
 * @brief Fuzz test with constrained input domains
 * 
 * Use domains to constrain the fuzzer to generate more meaningful inputs.
 */
void FuzzWithConstraints(
    int value,                  // Integer in a specific range
    float scale,                // Float in a specific range
    const std::string& name,    // String with specific characteristics
    bool flag) {                // Boolean
    
    try {
        // Your function call here
        // YourFunction(value, scale, name, flag);
        
    } catch (const std::exception&) {
        // Handle exceptions
    }
}
// Register with domain constraints
FUZZ_TEST(YourComponentFuzz, FuzzWithConstraints)
    .WithDomains(
        fuzztest::InRange(0, 1000),                              // value: 0-1000
        fuzztest::InRange(-10.0f, 10.0f),                       // scale: -10.0 to 10.0
        fuzztest::StringOf(fuzztest::InRange('a', 'z'))         // name: lowercase letters
            .WithSize(1, 50),                                    // length: 1-50
        fuzztest::Arbitrary<bool>()                              // flag: any bool
    );

// ============================================================================
// Example 3: Fuzz Test with Structured Data
// ============================================================================

/**
 * @brief Fuzz test with vectors and structured data
 */
void FuzzWithVectors(
    const std::vector<int>& int_vec,
    const std::vector<std::string>& string_vec) {
    
    try {
        // Process the vectors
        // YourFunction(int_vec, string_vec);
        
        // You can add invariant checks
        // EXPECT_LE(int_vec.size(), 1000);  // Should never exceed this
        
    } catch (const std::exception&) {
        // Handle exceptions
    }
}
FUZZ_TEST(YourComponentFuzz, FuzzWithVectors)
    .WithDomains(
        fuzztest::VectorOf(fuzztest::InRange(-100, 100))
            .WithSize(0, 100),                                   // 0-100 integers
        fuzztest::VectorOf(
            fuzztest::StringOf(fuzztest::InRange('a', 'z'))
                .WithMaxSize(20)
        ).WithSize(0, 50)                                        // 0-50 strings
    );

// ============================================================================
// Example 4: Fuzz Test with OneOf (Union Types)
// ============================================================================

/**
 * @brief Fuzz test that chooses from specific values
 */
void FuzzWithOneOf(
    const std::string& format_type,
    int channel) {
    
    try {
        // Your function that expects specific format types
        // YourFunction(format_type, channel);
        
    } catch (const std::exception&) {
        // Handle exceptions
    }
}
FUZZ_TEST(YourComponentFuzz, FuzzWithOneOf)
    .WithDomains(
        fuzztest::OneOf(
            fuzztest::Just("json"),                              // Specific values
            fuzztest::Just("xml"),
            fuzztest::Just("binary"),
            fuzztest::Arbitrary<std::string>().WithMaxSize(20)   // Or any string
        ),
        fuzztest::InRange(0, 16)                                 // channel: 0-15
    );

// ============================================================================
// Example 5: JSON Parsing Fuzz Test
// ============================================================================

#include "nlohmann/json.hpp"

/**
 * @brief Fuzz test for JSON parsing
 */
void FuzzJsonParsing(const std::string& json_string) {
    try {
        // Try to parse the JSON
        auto json_obj = nlohmann::json::parse(json_string);
        
        // If parsing succeeded, try to use it
        // YourJsonProcessor(json_obj);
        
    } catch (const nlohmann::json::parse_error&) {
        // JSON parsing failures are expected and acceptable
    } catch (const std::exception&) {
        // Other exceptions might indicate bugs
    }
}
FUZZ_TEST(YourComponentFuzz, FuzzJsonParsing);

// ============================================================================
// Example 6: File-based Fuzz Test
// ============================================================================

#include <filesystem>
#include <fstream>

/**
 * @brief Fuzz test that creates temporary files
 */
void FuzzFileLoading(const std::string& file_content) {
    try {
        // Create a temporary file
        std::string temp_file = std::tmpnam(nullptr);
        
        {
            std::ofstream file(temp_file);
            file << file_content;
        }
        
        // Try to load it with your function
        // YourFileLoader(temp_file);
        
        // Clean up
        std::filesystem::remove(temp_file);
        
    } catch (const std::exception&) {
        // Handle exceptions
    }
}
FUZZ_TEST(YourComponentFuzz, FuzzFileLoading)
    .WithDomains(
        fuzztest::Arbitrary<std::string>().WithMaxSize(10000)    // Limit file size
    );

// ============================================================================
// Common Domain Patterns
// ============================================================================

/*
Available domains (see FuzzTest documentation for complete list):

Integers:
- fuzztest::Arbitrary<int>()
- fuzztest::InRange(min, max)
- fuzztest::Positive<int>()
- fuzztest::NonNegative<int>()

Floats:
- fuzztest::Arbitrary<float>()
- fuzztest::InRange(min, max)
- fuzztest::Finite<float>()

Strings:
- fuzztest::Arbitrary<std::string>()
- fuzztest::StringOf(char_domain)
- fuzztest::PrintableAsciiString()
- fuzztest::AsciiString()

Containers:
- fuzztest::VectorOf(element_domain)
- fuzztest::ArrayOf<N>(element_domain)
- fuzztest::ContainerOf<Container>(element_domain)

Choices:
- fuzztest::Just(value)
- fuzztest::OneOf(domain1, domain2, ...)

Modifiers:
- .WithSize(min, max)         // For containers
- .WithMaxSize(size)           // For strings/containers
- .WithMinSize(size)           // For strings/containers

Custom:
- fuzztest::Map(func, domain) // Transform domain values
- fuzztest::Filter(pred, domain) // Filter domain values
*/

} // namespace
