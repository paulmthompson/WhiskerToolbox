/**
 * @file MockEditorTypes.hpp
 * @brief Lightweight mock EditorState types for registry/workspace fuzz testing
 *
 * Provides mock editor states with configurable field schemas for testing
 * EditorRegistry serialization without pulling in heavy widget dependencies.
 *
 * Phase 3 of the Workspace Fuzz Testing Roadmap.
 */

#ifndef FUZZ_MOCK_EDITOR_TYPES_HPP
#define FUZZ_MOCK_EDITOR_TYPES_HPP

#include "EditorState/EditorRegistry.hpp"
#include "EditorState/EditorState.hpp"

#include <rfl.hpp>
#include <rfl/json.hpp>

#include <memory>
#include <optional>
#include <string>
#include <vector>

// ============================================================================
// Mock StateData structs — reflect-cpp serializable, no Qt dependencies
// ============================================================================

/**
 * @brief Simple mock state data with primitive fields
 *
 * Tests basic round-trip of strings, ints, doubles, bools.
 */
struct MockStateDataA {
    std::string instance_id;
    std::string display_name = "MockA Widget";
    int int_field = 42;
    double double_field = 3.14;
    bool bool_field = false;
};

/**
 * @brief Mock state data with collections and optional fields
 *
 * Tests round-trip of vectors, optionals, and maps.
 */
struct MockStateDataB {
    std::string instance_id;
    std::string display_name = "MockB Widget";
    std::string string_field = "default";
    std::vector<std::string> list_field;
    std::optional<std::string> optional_field;
    std::optional<int> optional_int;
};

/**
 * @brief Mock state data with nested struct
 *
 * Tests round-trip of nested data structures.
 */
struct MockNestedData {
    double x = 0.0;
    double y = 0.0;
    std::string label = "origin";
};

struct MockStateDataC {
    std::string instance_id;
    std::string display_name = "MockC Widget";
    MockNestedData nested;
    std::vector<MockNestedData> nested_list;
};

// ============================================================================
// Mock EditorState classes — QObject wrappers around StateData structs
// ============================================================================

/**
 * @brief Mock EditorState with simple primitive fields (single-instance type)
 */
class MockEditorStateA : public EditorState {
    Q_OBJECT
public:
    explicit MockEditorStateA(QObject * parent = nullptr);

    [[nodiscard]] QString getTypeName() const override { return "MockTypeA"; }
    [[nodiscard]] std::string toJson() const override;
    bool fromJson(std::string const & json) override;

    // Typed setters for fuzz testing
    void setIntField(int value);
    void setDoubleField(double value);
    void setBoolField(bool value);

    [[nodiscard]] int intField() const { return _data.int_field; }
    [[nodiscard]] double doubleField() const { return _data.double_field; }
    [[nodiscard]] bool boolField() const { return _data.bool_field; }

private:
    MockStateDataA _data;
};

/**
 * @brief Mock EditorState with collections (multi-instance type)
 */
class MockEditorStateB : public EditorState {
    Q_OBJECT
public:
    explicit MockEditorStateB(QObject * parent = nullptr);

    [[nodiscard]] QString getTypeName() const override { return "MockTypeB"; }
    [[nodiscard]] std::string toJson() const override;
    bool fromJson(std::string const & json) override;

    // Typed setters for fuzz testing
    void setStringField(std::string const & value);
    void setListField(std::vector<std::string> const & value);
    void setOptionalField(std::optional<std::string> const & value);
    void setOptionalInt(std::optional<int> const & value);

    [[nodiscard]] std::string const & stringField() const { return _data.string_field; }
    [[nodiscard]] std::vector<std::string> const & listField() const { return _data.list_field; }
    [[nodiscard]] std::optional<std::string> const & optionalField() const { return _data.optional_field; }
    [[nodiscard]] std::optional<int> const & optionalInt() const { return _data.optional_int; }

private:
    MockStateDataB _data;
};

/**
 * @brief Mock EditorState with nested structs (single-instance type)
 */
class MockEditorStateC : public EditorState {
    Q_OBJECT
public:
    explicit MockEditorStateC(QObject * parent = nullptr);

    [[nodiscard]] QString getTypeName() const override { return "MockTypeC"; }
    [[nodiscard]] std::string toJson() const override;
    bool fromJson(std::string const & json) override;

    // Typed setters for fuzz testing
    void setNested(MockNestedData const & value);
    void setNestedList(std::vector<MockNestedData> const & value);

    [[nodiscard]] MockNestedData const & nested() const { return _data.nested; }
    [[nodiscard]] std::vector<MockNestedData> const & nestedList() const { return _data.nested_list; }

private:
    MockStateDataC _data;
};

// ============================================================================
// Registration helpers
// ============================================================================

/**
 * @brief Register all mock types with an EditorRegistry
 *
 * Registers MockTypeA (allow_multiple=false), MockTypeB (allow_multiple=true),
 * and MockTypeC (allow_multiple=false) with minimal factories.
 *
 * @param registry The registry to register types with
 */
void registerMockTypes(EditorRegistry * registry);

/**
 * @brief Register a subset of mock types
 *
 * @param registry The registry to register types with
 * @param count How many to register (1-3, clamped)
 */
void registerMockTypes(EditorRegistry * registry, int count);

#endif // FUZZ_MOCK_EDITOR_TYPES_HPP
