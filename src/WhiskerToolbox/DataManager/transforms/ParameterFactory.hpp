#ifndef PARAMETER_FACTORY_HPP
#define PARAMETER_FACTORY_HPP

#include "data_transforms.hpp"

#include "DataManager.hpp"

#include <nlohmann/json.hpp>
#include <functional>
#include <memory>
#include <string>
#include <unordered_map>
#include <typeinfo>
#include <any>


/**
 * @brief Function signature for parameter setters
 * 
 * @param param_obj Pointer to the parameter object
 * @param json_value JSON value to set
 * @param data_manager Pointer to data manager for resolving data references
 * @return true if parameter was set successfully
 */
using ParameterSetter = std::function<bool(TransformParametersBase* param_obj, 
                                          nlohmann::json const& json_value, 
                                          DataManager* data_manager)>;

/**
 * @brief Registry for parameter setters for different transform types and parameter names
 */
class ParameterFactory {
public:
    /**
     * @brief Register a parameter setter for a specific transform and parameter
     * 
     * @param transform_name Name of the transform
     * @param param_name Name of the parameter
     * @param setter Function to set the parameter value
     */
    void registerParameterSetter(std::string const& transform_name,
                                std::string const& param_name,
                                ParameterSetter setter);

    /**
     * @brief Set a parameter value with automatic type conversion
     * 
     * @param transform_name Name of the transform
     * @param param_obj Parameter object to modify
     * @param param_name Name of the parameter
     * @param json_value JSON value to set
     * @param data_manager Pointer to data manager for resolving references
     * @return true if parameter was set successfully
     */
    bool setParameter(std::string const& transform_name,
                     TransformParametersBase* param_obj,
                     std::string const& param_name,
                     nlohmann::json const& json_value,
                     DataManager* data_manager);

    /**
     * @brief Get the singleton instance of the parameter factory
     * 
     * @return ParameterFactory& Reference to the singleton instance
     */
    static ParameterFactory& getInstance();

    /**
     * @brief Initialize the factory with default parameter setters
     */
    void initializeDefaultSetters();

    /**
     * @brief Register a basic parameter (int, float, bool, string) with automatic type conversion
     * 
     * @tparam ParamClass The parameter class type
     * @tparam ParamType The type of the parameter member
     * @param transform_name Name of the transform
     * @param param_name Name of the parameter
     * @param param_member Pointer to the parameter member
     */
    template<typename ParamClass, typename ParamType>
    void registerBasicParameter(std::string const& transform_name,
                               std::string const& param_name,
                               ParamType ParamClass::* param_member);

    /**
     * @brief Register a data manager parameter (shared_ptr to data objects)
     * 
     * @tparam ParamClass The parameter class type
     * @tparam DataType The data type stored in DataManager
     * @param transform_name Name of the transform
     * @param param_name Name of the parameter
     * @param param_member Pointer to the parameter member
     */
    template<typename ParamClass, typename DataType>
    void registerDataParameter(std::string const& transform_name,
                              std::string const& param_name,
                              std::shared_ptr<DataType> ParamClass::* param_member);

    /**
     * @brief Register an enum parameter with string-to-enum mapping
     * 
     * @tparam ParamClass The parameter class type
     * @tparam EnumType The enum type
     * @param transform_name Name of the transform
     * @param param_name Name of the parameter
     * @param param_member Pointer to the parameter member
     * @param enum_map Map from string names to enum values
     */
    template<typename ParamClass, typename EnumType>
    void registerEnumParameter(std::string const& transform_name,
                              std::string const& param_name,
                              EnumType ParamClass::* param_member,
                              std::unordered_map<std::string, EnumType> const& enum_map);

private:
    std::unordered_map<std::string, std::unordered_map<std::string, ParameterSetter>> setters_;
    
    ParameterFactory() = default;
};

// Template implementations

template<typename ParamClass, typename ParamType>
void ParameterFactory::registerBasicParameter(std::string const& transform_name,
                                             std::string const& param_name,
                                             ParamType ParamClass::* param_member) {
    registerParameterSetter(transform_name, param_name,
        [param_member](TransformParametersBase* param_obj, nlohmann::json const& json_value, DataManager*) -> bool {
            auto* typed_params = static_cast<ParamClass*>(param_obj);
            
            if constexpr (std::is_arithmetic_v<ParamType> && !std::is_same_v<ParamType, bool>) {
                // Handle numeric types (int, float, double, etc.)
                if (json_value.is_number()) {
                    typed_params->*param_member = static_cast<ParamType>(json_value.get<double>());
                    return true;
                }
            } else if constexpr (std::is_same_v<ParamType, bool>) {
                // Handle boolean type
                if (json_value.is_boolean()) {
                    typed_params->*param_member = json_value.get<bool>();
                    return true;
                }
            } else if constexpr (std::is_same_v<ParamType, std::string>) {
                // Handle string type
                if (json_value.is_string()) {
                    typed_params->*param_member = json_value.get<std::string>();
                    return true;
                }
            }
            return false;
        });
}

template<typename ParamClass, typename DataType>
void ParameterFactory::registerDataParameter(std::string const& transform_name,
                                            std::string const& param_name,
                                            std::shared_ptr<DataType> ParamClass::* param_member) {
    registerParameterSetter(transform_name, param_name,
        [param_member](TransformParametersBase* param_obj, nlohmann::json const& json_value, DataManager* dm) -> bool {
            if (!json_value.is_string()) {
                return false;
            }
            
            std::string data_key = json_value.get<std::string>();
            auto data = dm->getData<DataType>(data_key);
            if (!data) {
                return false;
            }
            
            auto* typed_params = static_cast<ParamClass*>(param_obj);
            typed_params->*param_member = data;
            return true;
        });
}

template<typename ParamClass, typename EnumType>
void ParameterFactory::registerEnumParameter(std::string const& transform_name,
                                            std::string const& param_name,
                                            EnumType ParamClass::* param_member,
                                            std::unordered_map<std::string, EnumType> const& enum_map) {
    registerParameterSetter(transform_name, param_name,
        [param_member, enum_map](TransformParametersBase* param_obj, nlohmann::json const& json_value, DataManager*) -> bool {
            if (!json_value.is_string()) {
                return false;
            }
            
            std::string enum_str = json_value.get<std::string>();
            auto it = enum_map.find(enum_str);
            if (it == enum_map.end()) {
                return false;
            }
            
            auto* typed_params = static_cast<ParamClass*>(param_obj);
            typed_params->*param_member = it->second;
            return true;
        });
}

#endif // PARAMETER_FACTORY_HPP
