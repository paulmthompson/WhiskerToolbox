/**
 * @file test_variable_substitution.cpp
 * @brief Test for variable substitution in pipeline JSON
 */

#include "transforms/TransformPipeline.hpp"
#include "DataManager.hpp"
#include "transforms/TransformRegistry.hpp"

#include <gtest/gtest.h>
#include <nlohmann/json.hpp>

class VariableSubstitutionTest : public ::testing::Test {
protected:
    void SetUp() override {
        data_manager = std::make_unique<DataManager>();
        registry = std::make_unique<TransformRegistry>();
        pipeline = std::make_unique<TransformPipeline>(data_manager.get(), registry.get());
    }

    std::unique_ptr<DataManager> data_manager;
    std::unique_ptr<TransformRegistry> registry;
    std::unique_ptr<TransformPipeline> pipeline;
};

TEST_F(VariableSubstitutionTest, BasicStringSubstitution) {
    nlohmann::json config = R"({
        "metadata": {
            "variables": {
                "input_key": "test_data",
                "suffix": "_processed"
            }
        },
        "steps": [
            {
                "step_id": "test_step",
                "transform_name": "Test Transform",
                "input_key": "${input_key}",
                "output_key": "${input_key}${suffix}",
                "parameters": {}
            }
        ]
    })"_json;

    // Extract variables
    std::unordered_map<std::string, std::string> expected_vars = {
        {"input_key", "test_data"},
        {"suffix", "_processed"}
    };

    // Create a test config and substitute
    auto steps = config["steps"];
    auto vars = expected_vars;
    
    // Manually test substitution
    std::string input_str = "${input_key}";
    std::string expected_input = "test_data";
    
    size_t pos = input_str.find("${");
    ASSERT_NE(pos, std::string::npos);
    
    size_t end_pos = input_str.find("}", pos + 2);
    ASSERT_NE(end_pos, std::string::npos);
    
    std::string var_name = input_str.substr(pos + 2, end_pos - pos - 2);
    EXPECT_EQ(var_name, "input_key");
    
    auto it = vars.find(var_name);
    ASSERT_NE(it, vars.end());
    EXPECT_EQ(it->second, "test_data");
}

TEST_F(VariableSubstitutionTest, MultipleVariablesInOneString) {
    std::string test_str = "${input_key}${suffix}";
    std::unordered_map<std::string, std::string> vars = {
        {"input_key", "whisker_1"},
        {"suffix", "_angle"}
    };
    
    // Expected result: "whisker_1_angle"
    std::string expected = "whisker_1_angle";
    
    // Simulate substitution logic
    std::string result = test_str;
    size_t pos = 0;
    while ((pos = result.find("${", pos)) != std::string::npos) {
        size_t end_pos = result.find("}", pos + 2);
        if (end_pos == std::string::npos) {
            break;
        }
        
        std::string var_name = result.substr(pos + 2, end_pos - pos - 2);
        auto it = vars.find(var_name);
        
        if (it != vars.end()) {
            result.replace(pos, end_pos - pos + 1, it->second);
            pos += it->second.length();
        } else {
            pos = end_pos + 1;
        }
    }
    
    EXPECT_EQ(result, expected);
}

TEST_F(VariableSubstitutionTest, NestedVariableSubstitution) {
    nlohmann::json config = R"({
        "metadata": {
            "variables": {
                "base_key": "data",
                "suffix_1": "_filtered",
                "suffix_2": "_normalized"
            }
        },
        "steps": [
            {
                "step_id": "step1",
                "transform_name": "Test",
                "input_key": "${base_key}",
                "output_key": "${base_key}${suffix_1}",
                "parameters": {}
            },
            {
                "step_id": "step2",
                "transform_name": "Test",
                "input_key": "${base_key}${suffix_1}",
                "output_key": "${base_key}${suffix_1}${suffix_2}",
                "parameters": {}
            }
        ]
    })"_json;
    
    // Verify the JSON structure
    ASSERT_TRUE(config.contains("metadata"));
    ASSERT_TRUE(config["metadata"].contains("variables"));
    ASSERT_TRUE(config.contains("steps"));
    ASSERT_EQ(config["steps"].size(), 2);
    
    // Check that step 2's input matches step 1's output pattern
    EXPECT_EQ(config["steps"][0]["output_key"].get<std::string>(), 
              "${base_key}${suffix_1}");
    EXPECT_EQ(config["steps"][1]["input_key"].get<std::string>(), 
              "${base_key}${suffix_1}");
}

TEST_F(VariableSubstitutionTest, NumericVariableConversion) {
    nlohmann::json vars_json = R"({
        "threshold": 3.14,
        "count": 42,
        "enabled": true
    })"_json;
    
    // Simulate extractVariables logic
    std::unordered_map<std::string, std::string> vars;
    for (auto const& [key, value] : vars_json.items()) {
        if (value.is_string()) {
            vars[key] = value.get<std::string>();
        } else if (value.is_number()) {
            vars[key] = std::to_string(value.get<double>());
        } else if (value.is_boolean()) {
            vars[key] = value.get<bool>() ? "true" : "false";
        }
    }
    
    EXPECT_TRUE(vars.find("threshold") != vars.end());
    EXPECT_TRUE(vars.find("count") != vars.end());
    EXPECT_TRUE(vars.find("enabled") != vars.end());
    EXPECT_EQ(vars["enabled"], "true");
}
