#ifndef COMPUTER_REGISTRY_HPP
#define COMPUTER_REGISTRY_HPP

#include "utils/TableView/interfaces/IColumnComputer.h"
#include "utils/TableView/interfaces/IAnalogSource.h"
#include "utils/TableView/interfaces/IEventSource.h"
#include "utils/TableView/interfaces/IIntervalSource.h"
#include "utils/TableView/interfaces/IRowSelector.h"
#include "utils/TableView/ComputerRegistryTypes.hpp"
#include "utils/TableView/interfaces/IMultiColumnComputer.h"

#include <deque>
#include <functional>
#include <map>
#include <memory>
#include <optional>
#include <string>
#include <typeindex>
#include <variant>
#include <vector>

class DataManagerExtension;
class PointData;
class TimeFrame;

/**
 * @brief Abstract base for parameter descriptors that provide UI hints without Qt dependencies.
 */
class IParameterDescriptor {
public:
    IParameterDescriptor();
    virtual ~IParameterDescriptor();
    virtual std::string getName() const = 0;
    virtual std::string getDescription() const = 0;
    virtual bool isRequired() const = 0;
    virtual std::string getUIHint() const = 0;  // "enum", "text", "number", etc.
    virtual std::map<std::string, std::string> getUIProperties() const = 0;
    virtual std::unique_ptr<IParameterDescriptor> clone() const = 0;
};

/**
 * @brief Parameter descriptor for enumerated/choice parameters.
 */
class EnumParameterDescriptor : public IParameterDescriptor {
    std::string name_;
    std::string description_;
    std::vector<std::string> options_;
    std::string defaultValue_;
    bool required_;
    
public:
    EnumParameterDescriptor(std::string name, std::string description, 
                           std::vector<std::string> options, std::string defaultValue = "", 
                           bool required = true)
        : name_(std::move(name)), description_(std::move(description)), 
          options_(std::move(options)), defaultValue_(std::move(defaultValue)), 
          required_(required) {}
    
    std::string getName() const override { return name_; }
    std::string getDescription() const override { return description_; }
    bool isRequired() const override { return required_; }
    std::string getUIHint() const override { return "enum"; }
    
    std::map<std::string, std::string> getUIProperties() const override {
        std::map<std::string, std::string> props;
        
        // Join options with comma
        std::string optionsStr;
        for (size_t i = 0; i < options_.size(); ++i) {
            if (i > 0) optionsStr += ",";
            optionsStr += options_[i];
        }
        
        props["options"] = optionsStr;
        props["default"] = defaultValue_;
        return props;
    }
    
    std::unique_ptr<IParameterDescriptor> clone() const override {
        return std::make_unique<EnumParameterDescriptor>(name_, description_, options_, defaultValue_, required_);
    }
    
    const std::vector<std::string>& getOptions() const { return options_; }
    const std::string& getDefaultValue() const { return defaultValue_; }
};

/**
 * @brief Parameter descriptor for integer numeric parameters.
 */
class IntParameterDescriptor : public IParameterDescriptor {
    std::string name_;
    std::string description_;
    int defaultValue_;
    int minValue_;
    int maxValue_;
    bool required_;

public:
    IntParameterDescriptor(std::string name, std::string description,
                           int defaultValue = 0, int minValue = 0, int maxValue = 1000000,
                           bool required = true)
        : name_(std::move(name)), description_(std::move(description)),
          defaultValue_(defaultValue), minValue_(minValue), maxValue_(maxValue), required_(required) {}

    std::string getName() const override { return name_; }
    std::string getDescription() const override { return description_; }
    bool isRequired() const override { return required_; }
    std::string getUIHint() const override { return "number"; }

    std::map<std::string, std::string> getUIProperties() const override {
        return {
            {"default", std::to_string(defaultValue_)},
            {"min", std::to_string(minValue_)},
            {"max", std::to_string(maxValue_)}
        };
    }

    std::unique_ptr<IParameterDescriptor> clone() const override {
        return std::make_unique<IntParameterDescriptor>(name_, description_, defaultValue_, minValue_, maxValue_, required_);
    }
};

/**
 * @brief Non-templated base class for type-erased computer storage.
 * 
 * This allows us to store different templated IColumnComputer instances
 * in a single container with proper polymorphic destruction.
 */
class IComputerBase {
public:
    virtual ~IComputerBase() = default;
    
    // Make this class non-copyable and non-movable since it's a pure interface
    IComputerBase(IComputerBase const &) = delete;
    IComputerBase & operator=(IComputerBase const &) = delete;
    IComputerBase(IComputerBase &&) = delete;
    IComputerBase & operator=(IComputerBase &&) = delete;

protected:
    IComputerBase() = default;
};

/**
 * @brief Template wrapper that implements IComputerBase for specific computer types.
 */
template<typename T>
class ComputerWrapper : public IComputerBase {
public:
    explicit ComputerWrapper(std::unique_ptr<IColumnComputer<T>> computer)
        : computer_(std::move(computer)) {}
    
    IColumnComputer<T>* get() const { return computer_.get(); }
    
    /**
     * @brief Gets the underlying typed computer instance as a shared pointer.
     * @return Shared pointer to the typed computer interface.
     */
    std::shared_ptr<IColumnComputer<T>> getComputer() const {
        return std::shared_ptr<IColumnComputer<T>>(computer_.get(), [](IColumnComputer<T>*){});
    }
    
    /**
     * @brief Transfers ownership of the underlying computer.
     * @return Unique pointer to the typed computer interface.
     */
    std::unique_ptr<IColumnComputer<T>> releaseComputer() {
        return std::move(computer_);
    }
    
private:
    std::unique_ptr<IColumnComputer<T>> computer_;
};

/**
 * @brief Template wrapper to type-erase IMultiColumnComputer<T> in the registry.
 */
template<typename T>
class MultiComputerWrapper : public IComputerBase {
public:
    explicit MultiComputerWrapper(std::unique_ptr<IMultiColumnComputer<T>> computer)
        : computer_(std::move(computer)) {}

    IMultiColumnComputer<T>* get() const { return computer_.get(); }

    std::shared_ptr<IMultiColumnComputer<T>> getComputer() const {
        return std::shared_ptr<IMultiColumnComputer<T>>(computer_.get(), [](IMultiColumnComputer<T>*){});
    }

    std::unique_ptr<IMultiColumnComputer<T>> releaseComputer() {
        return std::move(computer_);
    }

private:
    std::unique_ptr<IMultiColumnComputer<T>> computer_;
};



/**
 * @brief Information about available computer parameters.
 */
struct ComputerParameterInfo {
    std::string name;           ///< Parameter name
    std::string description;    ///< Human-readable description
    std::type_index type;       ///< Type of the parameter
    bool isRequired;            ///< Whether the parameter is required
    std::string defaultValue;   ///< String representation of default value (if any)
    
    ComputerParameterInfo();
    ComputerParameterInfo(std::string name_, std::string description_, std::type_index type_, bool required = false, std::string defaultValue_ = "");

    ~ComputerParameterInfo();
};

/**
 * @brief Information about an available computer.
 */
struct ComputerInfo {
    std::string name;                                   ///< Display name for the computer
    std::string description;                            ///< Human-readable description
    std::type_index outputType;                         ///< Type of the computed output
    std::string outputTypeName;                         ///< Human-readable name of the output type
    bool isVectorType;                                  ///< True if output type is std::vector<T>
    std::type_index elementType;                        ///< For vector types, the element type; same as outputType for non-vectors
    std::string elementTypeName;                        ///< Human-readable name of the element type
    RowSelectorType requiredRowSelector;                ///< Required row selector type
    std::type_index requiredSourceType;                 ///< Required data source interface type
    std::vector<std::unique_ptr<IParameterDescriptor>> parameterDescriptors; ///< Parameter descriptors for UI generation
    bool isMultiOutput = false;                         ///< True if computer produces multiple outputs of same type
    // Optional factory to derive output suffixes from parameters for discovery/UI
    std::function<std::vector<std::string>(std::map<std::string, std::string> const&)> makeOutputSuffixes;
    
    // Default constructor
    ComputerInfo() 
        : name(), description(), outputType(typeid(void)), outputTypeName("void"),
          isVectorType(false), elementType(typeid(void)), elementTypeName("void"),
          requiredRowSelector(RowSelectorType::Interval), requiredSourceType(typeid(void)), parameterDescriptors() {}
    
    // Helper constructor for simple types
    ComputerInfo(std::string name_, std::string description_, std::type_index outputType_, 
                 std::string outputTypeName_, RowSelectorType rowSelector_, std::type_index sourceType_)
        : name(std::move(name_)), description(std::move(description_)), 
          outputType(outputType_), outputTypeName(std::move(outputTypeName_)),
          isVectorType(false), elementType(outputType_), elementTypeName(outputTypeName_),
          requiredRowSelector(rowSelector_), requiredSourceType(sourceType_), parameterDescriptors() {}

    // Constructor with parameter descriptors for simple types
    ComputerInfo(std::string name_, std::string description_, std::type_index outputType_, 
                 std::string outputTypeName_, RowSelectorType rowSelector_, std::type_index sourceType_,
                 std::vector<std::unique_ptr<IParameterDescriptor>> parameterDescriptors_)
        : name(std::move(name_)), description(std::move(description_)), 
          outputType(outputType_), outputTypeName(std::move(outputTypeName_)),
          isVectorType(false), elementType(outputType_), elementTypeName(outputTypeName_),
          requiredRowSelector(rowSelector_), requiredSourceType(sourceType_), 
          parameterDescriptors(std::move(parameterDescriptors_)) {}
    
    // Helper constructor for vector types
    ComputerInfo(std::string name_, std::string description_, std::type_index outputType_, 
                 std::string outputTypeName_, std::type_index elementType_, std::string elementTypeName_,
                 RowSelectorType rowSelector_, std::type_index sourceType_)
        : name(std::move(name_)), description(std::move(description_)), 
          outputType(outputType_), outputTypeName(std::move(outputTypeName_)),
          isVectorType(true), elementType(elementType_), elementTypeName(std::move(elementTypeName_)),
          requiredRowSelector(rowSelector_), requiredSourceType(sourceType_), parameterDescriptors() {}

    // Helper constructor for vector types with parameter descriptors
    ComputerInfo(std::string name_, std::string description_, std::type_index outputType_, 
                 std::string outputTypeName_, std::type_index elementType_, std::string elementTypeName_,
                 RowSelectorType rowSelector_, std::type_index sourceType_,
                 std::vector<std::unique_ptr<IParameterDescriptor>> parameterDescriptors_)
        : name(std::move(name_)), description(std::move(description_)),
          outputType(outputType_), outputTypeName(std::move(outputTypeName_)),
          isVectorType(true), elementType(elementType_), elementTypeName(std::move(elementTypeName_)),
          requiredRowSelector(rowSelector_), requiredSourceType(sourceType_),
          parameterDescriptors(std::move(parameterDescriptors_)) {}
    
    // Helper methods
    bool hasParameters() const { return !parameterDescriptors.empty(); }
    
    // Copy constructor
    ComputerInfo(const ComputerInfo& other)
        : name(other.name), description(other.description), 
          outputType(other.outputType), outputTypeName(other.outputTypeName),
          isVectorType(other.isVectorType), elementType(other.elementType), 
          elementTypeName(other.elementTypeName),
          requiredRowSelector(other.requiredRowSelector), 
          requiredSourceType(other.requiredSourceType),
          isMultiOutput(other.isMultiOutput),
          makeOutputSuffixes(other.makeOutputSuffixes) {
        // Deep copy parameter descriptors
        parameterDescriptors.reserve(other.parameterDescriptors.size());
        for (const auto& param : other.parameterDescriptors) {
            parameterDescriptors.push_back(param->clone());
        }
    }
    
    // Copy assignment operator
    ComputerInfo& operator=(const ComputerInfo& other) {
        if (this != &other) {
            name = other.name;
            description = other.description;
            outputType = other.outputType;
            outputTypeName = other.outputTypeName;
            isVectorType = other.isVectorType;
            elementType = other.elementType;
            elementTypeName = other.elementTypeName;
            requiredRowSelector = other.requiredRowSelector;
            requiredSourceType = other.requiredSourceType;
            isMultiOutput = other.isMultiOutput;
            makeOutputSuffixes = other.makeOutputSuffixes;
            
            // Deep copy parameter descriptors
            parameterDescriptors.clear();
            parameterDescriptors.reserve(other.parameterDescriptors.size());
            for (const auto& param : other.parameterDescriptors) {
                parameterDescriptors.push_back(param->clone());
            }
        }
        return *this;
    }
    
    // Move constructor and assignment are automatically generated and work correctly
    
    // Legacy convenience constructor for backward compatibility
    ComputerInfo(std::string name_, std::string description_, std::type_index outputType_, 
                 RowSelectorType rowSelector, std::type_index sourceType, 
                 std::vector<ComputerParameterInfo> params = {})
        : name(std::move(name_)), description(std::move(description_)), outputType(outputType_),
          outputTypeName("unknown"), isVectorType(false), elementType(outputType_), elementTypeName("unknown"),
          requiredRowSelector(rowSelector), requiredSourceType(sourceType), parameterDescriptors() {}
};

/**
 * @brief Factory function type for creating computer instances.
 * 
 * The function takes a data source variant and a map of parameter values,
 * and returns a type-erased computer instance.
 */
using ComputerFactory = std::function<std::unique_ptr<IComputerBase>(
    DataSourceVariant const& source,
    std::map<std::string, std::string> const& parameters
)>;

    using MultiComputerFactory = std::function<std::unique_ptr<IComputerBase>(
        DataSourceVariant const& source,
        std::map<std::string, std::string> const& parameters
    )>;

/**
 * @brief Information about an available adapter.
 */
struct AdapterInfo {
    std::string name;                                   ///< Display name for the adapter
    std::string description;                            ///< Human-readable description
    std::type_index inputType;                          ///< Input data type (e.g., typeid(PointData))
    std::type_index outputType;                         ///< Output interface type (e.g., typeid(IAnalogSource))
    std::vector<ComputerParameterInfo> parameters;     ///< Adapter-specific parameters
    
    // Default constructor
    AdapterInfo() 
        : name(), description(), inputType(typeid(void)), outputType(typeid(void)), parameters() {}
    
    // Convenience constructor
    AdapterInfo(std::string name_, std::string description_, std::type_index inputType_, 
                std::type_index outputType_, std::vector<ComputerParameterInfo> params = {})
        : name(std::move(name_)), description(std::move(description_)), 
          inputType(inputType_), outputType(outputType_), parameters(std::move(params)) {}
};

/**
 * @brief Factory function type for creating adapter instances.
 */
using AdapterFactory = std::function<DataSourceVariant(
    std::shared_ptr<void> const& sourceData,
    std::shared_ptr<TimeFrame> const& timeFrame,
    std::string const& name,
    std::map<std::string, std::string> const& parameters
)>;

/**
 * @brief Registry for TableView column computers and data adapters.
 * 
 * This registry manages the available computers that can generate table columns
 * from different data source types and row selector combinations. It also
 * manages adapters that can convert raw data types into interface types
 * suitable for use with computers.
 */
class ComputerRegistry {
public:
    ComputerRegistry();

    // Make registry non-copyable and non-movable
    ComputerRegistry(ComputerRegistry const &) = delete;
    ComputerRegistry & operator=(ComputerRegistry const &) = delete;
    ComputerRegistry(ComputerRegistry &&) = delete;
    ComputerRegistry & operator=(ComputerRegistry &&) = delete;

    /**
     * @brief Gets available computers for a specific row selector and data source combination.
     * @param rowSelectorType The type of row selector being used.
     * @param dataSource The data source variant.
     * @return Vector of ComputerInfo describing available computers.
     */
    std::vector<ComputerInfo> getAvailableComputers(
        RowSelectorType rowSelectorType,
        DataSourceVariant const& dataSource
    ) const;

    /**
     * @brief Gets available adapters for a specific data type.
     * @param dataType The type index of the raw data type.
     * @return Vector of AdapterInfo describing available adapters.
     */
    std::vector<AdapterInfo> getAvailableAdapters(std::type_index dataType) const;

    /**
     * @brief Creates a computer instance by name.
     * @param computerName The name of the computer to create.
     * @param dataSource The data source to use.
     * @param parameters Map of parameter name -> string value.
     * @return Type-erased unique_ptr to the computer instance, or nullptr if creation failed.
     */
    std::unique_ptr<IComputerBase> createComputer(
        std::string const& computerName,
        DataSourceVariant const& dataSource,
        std::map<std::string, std::string> const& parameters = {}
    ) const;

    std::unique_ptr<IComputerBase> createMultiComputer(
        std::string const& computerName,
        DataSourceVariant const& dataSource,
        std::map<std::string, std::string> const& parameters = {}
    ) const;

    /**
     * @brief Creates an adapter instance by name.
     * @param adapterName The name of the adapter to create.
     * @param sourceData The raw data to adapt.
     * @param timeFrame The time frame for the adapted data.
     * @param name The name for the adapted data source.
     * @param parameters Map of parameter name -> string value.
     * @return DataSourceVariant containing the adapted interface, or empty variant if creation failed.
     */
    DataSourceVariant createAdapter(
        std::string const& adapterName,
        std::shared_ptr<void> const& sourceData,
        std::shared_ptr<TimeFrame> const& timeFrame,
        std::string const& name,
        std::map<std::string, std::string> const& parameters = {}
    ) const;

    /**
     * @brief Finds computer info by name.
     * @param computerName The name of the computer.
     * @return Pointer to ComputerInfo, or nullptr if not found.
     */
    ComputerInfo const* findComputerInfo(std::string const& computerName) const;

    /**
     * @brief Finds adapter info by name.
     * @param adapterName The name of the adapter.
     * @return Pointer to AdapterInfo, or nullptr if not found.
     */
    AdapterInfo const* findAdapterInfo(std::string const& adapterName) const;

    /**
     * @brief Gets all registered computer names.
     * @return Vector of all computer names.
     */
    std::vector<std::string> getAllComputerNames() const;

    /**
     * @brief Gets all registered adapter names.
     * @return Vector of all adapter names.
     */
    std::vector<std::string> getAllAdapterNames() const;

    // Enhanced type discovery methods
    
    /**
     * @brief Gets all available output types that computers can produce.
     * @return Vector of type_index values representing all possible output types.
     */
    std::vector<std::type_index> getAvailableOutputTypes() const;

    /**
     * @brief Gets human-readable names for available output types.
     * @return Map from type_index to human-readable type name.
     */
    std::map<std::type_index, std::string> getOutputTypeNames() const;

    /**
     * @brief Gets computers that can produce a specific output type.
     * @param outputType The desired output type.
     * @param rowSelectorType Optional filter by row selector type.
     * @param sourceType Optional filter by required source type.
     * @return Vector of ComputerInfo for computers matching the criteria.
     */
    std::vector<ComputerInfo> getComputersByOutputType(
        std::type_index outputType,
        std::optional<RowSelectorType> rowSelectorType = std::nullopt,
        std::optional<std::type_index> sourceType = std::nullopt
    ) const;

    /**
     * @brief Checks if a computer supports vector output types.
     * @param computerName The name of the computer to check.
     * @return True if the computer outputs vector types, false otherwise.
     */
    bool isVectorComputer(std::string const& computerName) const;

    /**
     * @brief Gets the element type for vector-output computers.
     * @param computerName The name of the computer.
     * @return type_index of the element type, or typeid(void) if not a vector computer.
     */
    std::type_index getElementType(std::string const& computerName) const;

    /**
     * @brief Creates a type-safe computer with known output type.
     * 
     * This template method provides compile-time type safety when the caller
     * knows the expected output type. It returns a properly typed computer
     * interface rather than a type-erased one.
     * 
     * @tparam T The expected output type of the computer.
     * @param computerName The name of the computer to create.
     * @param dataSource The data source to use.
     * @param parameters Map of parameter name -> string value.
     * @return Unique pointer to IColumnComputer<T>, or nullptr if creation failed or type mismatch.
     */
    template<typename T>
    std::unique_ptr<IColumnComputer<T>> createTypedComputer(
        std::string const& computerName,
        DataSourceVariant const& dataSource,
        std::map<std::string, std::string> const& parameters = {}
    ) const {
        auto info = findComputerInfo(computerName);
        if (!info || info->outputType != typeid(T)) {
            return nullptr;
        }
        
        auto base_computer = createComputer(computerName, dataSource, parameters);
        if (!base_computer) {
            return nullptr;
        }
        
        // Safe cast since we verified the type
        auto wrapper = dynamic_cast<ComputerWrapper<T>*>(base_computer.get());
        if (!wrapper) {
            return nullptr;
        }
        
        base_computer.release(); // Transfer ownership
        return wrapper->releaseComputer(); // Use releaseComputer() to get unique_ptr
    }

    template<typename T>
    std::unique_ptr<IMultiColumnComputer<T>> createTypedMultiComputer(
        std::string const& computerName,
        DataSourceVariant const& dataSource,
        std::map<std::string, std::string> const& parameters = {}
    ) const {
        auto info = findComputerInfo(computerName);
        if (!info || info->outputType != typeid(T) || !info->isMultiOutput) {
            return nullptr;
        }

        auto base_computer = createMultiComputer(computerName, dataSource, parameters);
        if (!base_computer) {
            return nullptr;
        }

        auto wrapper = dynamic_cast<MultiComputerWrapper<T>*>(base_computer.get());
        if (!wrapper) {
            return nullptr;
        }

        base_computer.release();
        return wrapper->releaseComputer();
    }

private:
    // Computer registration storage
    std::deque<ComputerInfo> all_computers_;
    std::map<std::string, ComputerFactory> computer_factories_;
    std::map<std::string, MultiComputerFactory> multi_computer_factories_;
    
    // Maps (RowSelectorType, SourceTypeIndex) -> vector<ComputerInfo*>
    std::map<std::pair<RowSelectorType, std::type_index>, std::vector<ComputerInfo const*>> selector_source_to_computers_;
    
    // Computer name lookup
    std::map<std::string, ComputerInfo const*> name_to_computer_;

    // Adapter registration storage
    std::deque<AdapterInfo> all_adapters_;
    std::map<std::string, AdapterFactory> adapter_factories_;
    
    // Maps input type_index -> vector<AdapterInfo*>
    std::map<std::type_index, std::vector<AdapterInfo const*>> input_type_to_adapters_;
    
    // Adapter name lookup
    std::map<std::string, AdapterInfo const*> name_to_adapter_;

    /**
     * @brief Registers a computer with the registry.
     * @param info ComputerInfo describing the computer.
     * @param factory Factory function for creating instances.
     */
    void registerComputer(ComputerInfo info, ComputerFactory factory);

    void registerMultiComputer(ComputerInfo info, MultiComputerFactory factory);

    /**
     * @brief Registers an adapter with the registry.
     * @param info AdapterInfo describing the adapter.
     * @param factory Factory function for creating instances.
     */
    void registerAdapter(AdapterInfo info, AdapterFactory factory);

    /**
     * @brief Pre-computes the mapping from (row selector, source type) to computers.
     */
    void computeComputerMappings();

    /**
     * @brief Pre-computes the mapping from input types to adapters.
     */
    void computeAdapterMappings();

    /**
     * @brief Helper function to get the type index from a DataSourceVariant.
     */
    std::type_index getSourceTypeIndex(DataSourceVariant const& source) const;

    /**
     * @brief Registers all built-in computers.
     */
    void registerBuiltInComputers();

    /**
     * @brief Registers all built-in adapters.
     */
    void registerBuiltInAdapters();
};

#endif // COMPUTER_REGISTRY_HPP
