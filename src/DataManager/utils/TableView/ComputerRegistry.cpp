#include "ComputerRegistry.hpp"

#include "Lines/Line_Data.hpp"
#include "Points/Point_Data.hpp"
#include "adapters/LineDataAdapter.h"
#include "adapters/PointComponentAdapter.h"
#include "computers/AnalogSliceGathererComputer.h"
#include "computers/AnalogTimestampOffsetsMultiComputer.h"
#include "computers/EventInIntervalComputer.h"
#include "computers/IntervalOverlapComputer.h"
#include "computers/IntervalPropertyComputer.h"
#include "computers/IntervalReductionComputer.h"
#include "computers/LineSamplingMultiComputer.h"
#include "computers/TimestampInIntervalComputer.h"
#include "computers/TimestampValueComputer.h"
#include "interfaces/IEventSource.h"
#include "interfaces/ILineSource.h"

#include <iostream>
#include <optional>
#include <set>
#include <sstream>

IParameterDescriptor::IParameterDescriptor() = default;

IParameterDescriptor::~IParameterDescriptor() = default;

ComputerParameterInfo::ComputerParameterInfo()
    : name(),
      description(),
      type(typeid(void)),
      isRequired(false),
      defaultValue() {}

ComputerParameterInfo::ComputerParameterInfo(std::string name_, std::string description_, std::type_index type_, bool required, std::string defaultValue_)
    : name(std::move(name_)),
      description(std::move(description_)),
      type(type_),
      isRequired(required),
      defaultValue(std::move(defaultValue_)) {}

ComputerParameterInfo::~ComputerParameterInfo() = default;

ComputerRegistry::ComputerRegistry() {
    //std::cout << "Initializing Computer Registry..." << std::endl;

    registerBuiltInComputers();
    registerBuiltInAdapters();

    computeComputerMappings();
    computeAdapterMappings();

    /*
    std::cout << "Computer Registry Initialized with " 
              << all_computers_.size() << " computers and " 
              << all_adapters_.size() << " adapters." << std::endl;
    */
}

std::vector<ComputerInfo> ComputerRegistry::getAvailableComputers(
        RowSelectorType rowSelectorType,
        DataSourceVariant const & dataSource) const {

    auto sourceTypeIndex = getSourceTypeIndex(dataSource);
    auto key = std::make_pair(rowSelectorType, sourceTypeIndex);

    auto it = selector_source_to_computers_.find(key);
    if (it != selector_source_to_computers_.end()) {
        std::vector<ComputerInfo> result;
        result.reserve(it->second.size());

        for (auto const * computerInfo: it->second) {
            result.push_back(*computerInfo);
        }

        return result;
    }

    return {};// No computers available for this combination
}

std::vector<AdapterInfo> ComputerRegistry::getAvailableAdapters(std::type_index dataType) const {
    auto it = input_type_to_adapters_.find(dataType);
    if (it != input_type_to_adapters_.end()) {
        std::vector<AdapterInfo> result;
        result.reserve(it->second.size());

        for (auto const * adapterInfo: it->second) {
            result.push_back(*adapterInfo);
        }

        return result;
    }

    return {};// No adapters available for this type
}

std::unique_ptr<IComputerBase> ComputerRegistry::createComputer(
        std::string const & computerName,
        DataSourceVariant const & dataSource,
        std::map<std::string, std::string> const & parameters) const {

    auto it = computer_factories_.find(computerName);
    if (it != computer_factories_.end()) {
        try {
            return it->second(dataSource, parameters);
        } catch (std::exception const & e) {
            std::cerr << "Error creating computer '" << computerName << "': " << e.what() << std::endl;
            return nullptr;
        }
    }

    std::cerr << "Computer '" << computerName << "' not found in registry." << std::endl;
    return nullptr;
}

std::unique_ptr<IComputerBase> ComputerRegistry::createMultiComputer(
        std::string const & computerName,
        DataSourceVariant const & dataSource,
        std::map<std::string, std::string> const & parameters) const {
    auto it = multi_computer_factories_.find(computerName);
    if (it != multi_computer_factories_.end()) {
        try {
            return it->second(dataSource, parameters);
        } catch (std::exception const & e) {
            std::cerr << "Error creating multi-computer '" << computerName << "': " << e.what() << std::endl;
            return nullptr;
        }
    }
    std::cerr << "Multi-computer '" << computerName << "' not found in registry." << std::endl;
    return nullptr;
}

DataSourceVariant ComputerRegistry::createAdapter(
        std::string const & adapterName,
        std::shared_ptr<void> const & sourceData,
        std::shared_ptr<TimeFrame> const & timeFrame,
        std::string const & name,
        std::map<std::string, std::string> const & parameters) const {

    auto it = adapter_factories_.find(adapterName);
    if (it != adapter_factories_.end()) {
        try {
            return it->second(sourceData, timeFrame, name, parameters);
        } catch (std::exception const & e) {
            std::cerr << "Error creating adapter '" << adapterName << "': " << e.what() << std::endl;
            return DataSourceVariant{};
        }
    }

    std::cerr << "Adapter '" << adapterName << "' not found in registry." << std::endl;
    return DataSourceVariant{};
}

ComputerInfo const * ComputerRegistry::findComputerInfo(std::string const & computerName) const {
    auto it = name_to_computer_.find(computerName);
    return (it != name_to_computer_.end()) ? it->second : nullptr;
}

AdapterInfo const * ComputerRegistry::findAdapterInfo(std::string const & adapterName) const {
    auto it = name_to_adapter_.find(adapterName);
    return (it != name_to_adapter_.end()) ? it->second : nullptr;
}

std::vector<std::string> ComputerRegistry::getAllComputerNames() const {
    std::vector<std::string> names;
    names.reserve(all_computers_.size());

    for (auto const & info: all_computers_) {
        names.push_back(info.name);
    }

    return names;
}

std::vector<std::string> ComputerRegistry::getAllAdapterNames() const {
    std::vector<std::string> names;
    names.reserve(all_adapters_.size());

    for (auto const & info: all_adapters_) {
        names.push_back(info.name);
    }

    return names;
}

std::vector<std::type_index> ComputerRegistry::getAvailableOutputTypes() const {
    std::set<std::type_index> unique_types;

    for (auto const & info: all_computers_) {
        unique_types.insert(info.outputType);
    }

    return std::vector<std::type_index>(unique_types.begin(), unique_types.end());
}

std::map<std::type_index, std::string> ComputerRegistry::getOutputTypeNames() const {
    std::map<std::type_index, std::string> type_names;

    for (auto const & info: all_computers_) {
        type_names[info.outputType] = info.outputTypeName;
    }

    return type_names;
}

std::vector<ComputerInfo> ComputerRegistry::getComputersByOutputType(
        std::type_index outputType,
        std::optional<RowSelectorType> rowSelectorType,
        std::optional<std::type_index> sourceType) const {
    std::vector<ComputerInfo> result;

    for (auto const & info: all_computers_) {
        if (info.outputType != outputType) {
            continue;
        }

        if (rowSelectorType && info.requiredRowSelector != *rowSelectorType) {
            continue;
        }

        if (sourceType && info.requiredSourceType != *sourceType) {
            continue;
        }

        result.push_back(info);
    }

    return result;
}

bool ComputerRegistry::isVectorComputer(std::string const & computerName) const {
    auto info = findComputerInfo(computerName);
    return info ? info->isVectorType : false;
}

std::type_index ComputerRegistry::getElementType(std::string const & computerName) const {
    auto info = findComputerInfo(computerName);
    return info ? info->elementType : typeid(void);
}

void ComputerRegistry::registerComputer(ComputerInfo info, ComputerFactory factory) {
    std::string const name = info.name;// Copy the name before moving

    if (name_to_computer_.count(name)) {
        std::cerr << "Warning: Computer '" << name << "' already registered." << std::endl;
        return;
    }

    /*
    std::cout << "Registering computer: " << name 
              << " (Row selector: " << static_cast<int>(info.requiredRowSelector)
              << ", Source type: " << info.requiredSourceType.name() << ")" << std::endl;
    */
    all_computers_.push_back(std::move(info));
    ComputerInfo const * infoPtr = &all_computers_.back();

    name_to_computer_[name] = infoPtr;
    computer_factories_[name] = std::move(factory);
}

void ComputerRegistry::registerAdapter(AdapterInfo info, AdapterFactory factory) {
    std::string const name = info.name;// Copy the name before moving

    if (name_to_adapter_.count(name)) {
        std::cerr << "Warning: Adapter '" << name << "' already registered." << std::endl;
        return;
    }

    /*
    std::cout << "Registering adapter: " << name 
              << " (Input type: " << info.inputType.name()
              << ", Output type: " << info.outputType.name() << ")" << std::endl;
    */
    all_adapters_.push_back(std::move(info));
    AdapterInfo const * infoPtr = &all_adapters_.back();

    name_to_adapter_[name] = infoPtr;
    adapter_factories_[name] = std::move(factory);
}

void ComputerRegistry::registerMultiComputer(ComputerInfo info, MultiComputerFactory factory) {
    std::string const name = info.name;
    if (name_to_computer_.count(name) || multi_computer_factories_.count(name)) {
        std::cerr << "Warning: Computer '" << name << "' already registered." << std::endl;
        return;
    }

    info.isMultiOutput = true;

    /*
    std::cout << "Registering multi-output computer: " << name
              << " (Row selector: " << static_cast<int>(info.requiredRowSelector)
              << ", Source type: " << info.requiredSourceType.name() << ")" << std::endl;
    */
    all_computers_.push_back(std::move(info));
    ComputerInfo const * infoPtr = &all_computers_.back();

    name_to_computer_[name] = infoPtr;
    multi_computer_factories_[name] = std::move(factory);
}

void ComputerRegistry::computeComputerMappings() {
    //std::cout << "Computing computer mappings..." << std::endl;
    selector_source_to_computers_.clear();

    for (auto const & info: all_computers_) {
        auto key = std::make_pair(info.requiredRowSelector, info.requiredSourceType);
        selector_source_to_computers_[key].push_back(&info);
    }

    //std::cout << "Finished computing computer mappings." << std::endl;
}

void ComputerRegistry::computeAdapterMappings() {
    //std::cout << "Computing adapter mappings..." << std::endl;
    input_type_to_adapters_.clear();

    for (auto const & info: all_adapters_) {
        input_type_to_adapters_[info.inputType].push_back(&info);
    }

    //std::cout << "Finished computing adapter mappings." << std::endl;
}

std::type_index ComputerRegistry::getSourceTypeIndex(DataSourceVariant const & source) const {
    return std::visit([](auto const & src) -> std::type_index {
        return typeid(src);
    },
                      source);
}

void ComputerRegistry::registerBuiltInComputers() {
    //std::cout << "Registering built-in computers..." << std::endl;

    // IntervalReductionComputer - Mean
    {
        ComputerInfo info("Interval Mean",
                          "Calculate mean value over intervals",
                          typeid(double),
                          "double",
                          RowSelectorType::IntervalBased,
                          typeid(std::shared_ptr<IAnalogSource>));

        ComputerFactory factory = [](DataSourceVariant const & source,
                                     std::map<std::string, std::string> const &) -> std::unique_ptr<IComputerBase> {
            if (auto analogSrc = std::get_if<std::shared_ptr<IAnalogSource>>(&source)) {
                auto computer = std::make_unique<IntervalReductionComputer>(*analogSrc, ReductionType::Mean);
                return std::make_unique<ComputerWrapper<double>>(std::move(computer));
            }
            return nullptr;
        };

        registerComputer(std::move(info), std::move(factory));
    }

    // IntervalReductionComputer - Max
    {
        ComputerInfo info("Interval Max",
                          "Calculate maximum value over intervals",
                          typeid(double),
                          "double",
                          RowSelectorType::IntervalBased,
                          typeid(std::shared_ptr<IAnalogSource>));

        ComputerFactory factory = [](DataSourceVariant const & source,
                                     std::map<std::string, std::string> const &) -> std::unique_ptr<IComputerBase> {
            if (auto analogSrc = std::get_if<std::shared_ptr<IAnalogSource>>(&source)) {
                auto computer = std::make_unique<IntervalReductionComputer>(*analogSrc, ReductionType::Max);
                return std::make_unique<ComputerWrapper<double>>(std::move(computer));
            }
            return nullptr;
        };

        registerComputer(std::move(info), std::move(factory));
    }

    // IntervalReductionComputer - Min
    {
        ComputerInfo info("Interval Min",
                          "Calculate minimum value over intervals",
                          typeid(double),
                          "double",
                          RowSelectorType::IntervalBased,
                          typeid(std::shared_ptr<IAnalogSource>));

        ComputerFactory factory = [](DataSourceVariant const & source,
                                     std::map<std::string, std::string> const &) -> std::unique_ptr<IComputerBase> {
            if (auto analogSrc = std::get_if<std::shared_ptr<IAnalogSource>>(&source)) {
                auto computer = std::make_unique<IntervalReductionComputer>(*analogSrc, ReductionType::Min);
                return std::make_unique<ComputerWrapper<double>>(std::move(computer));
            }
            return nullptr;
        };

        registerComputer(std::move(info), std::move(factory));
    }

    // IntervalReductionComputer - StdDev
    {
        ComputerInfo info("Interval Standard Deviation",
                          "Calculate standard deviation over intervals",
                          typeid(double),
                          "double",
                          RowSelectorType::IntervalBased,
                          typeid(std::shared_ptr<IAnalogSource>));

        ComputerFactory factory = [](DataSourceVariant const & source,
                                     std::map<std::string, std::string> const &) -> std::unique_ptr<IComputerBase> {
            if (auto analogSrc = std::get_if<std::shared_ptr<IAnalogSource>>(&source)) {
                auto computer = std::make_unique<IntervalReductionComputer>(*analogSrc, ReductionType::StdDev);
                return std::make_unique<ComputerWrapper<double>>(std::move(computer));
            }
            return nullptr;
        };

        registerComputer(std::move(info), std::move(factory));
    }

    // IntervalReductionComputer - Sum
    {
        ComputerInfo info("Interval Sum",
                          "Calculate sum of values over intervals",
                          typeid(double),
                          "double",
                          RowSelectorType::IntervalBased,
                          typeid(std::shared_ptr<IAnalogSource>));

        ComputerFactory factory = [](DataSourceVariant const & source,
                                     std::map<std::string, std::string> const &) -> std::unique_ptr<IComputerBase> {
            if (auto analogSrc = std::get_if<std::shared_ptr<IAnalogSource>>(&source)) {
                auto computer = std::make_unique<IntervalReductionComputer>(*analogSrc, ReductionType::Sum);
                return std::make_unique<ComputerWrapper<double>>(std::move(computer));
            }
            return nullptr;
        };

        registerComputer(std::move(info), std::move(factory));
    }

    // IntervalReductionComputer - Count
    {
        ComputerInfo info("Interval Count",
                          "Count number of values over intervals",
                          typeid(double),
                          "double",
                          RowSelectorType::IntervalBased,
                          typeid(std::shared_ptr<IAnalogSource>));

        ComputerFactory factory = [](DataSourceVariant const & source,
                                     std::map<std::string, std::string> const &) -> std::unique_ptr<IComputerBase> {
            if (auto analogSrc = std::get_if<std::shared_ptr<IAnalogSource>>(&source)) {
                auto computer = std::make_unique<IntervalReductionComputer>(*analogSrc, ReductionType::Count);
                return std::make_unique<ComputerWrapper<double>>(std::move(computer));
            }
            return nullptr;
        };

        registerComputer(std::move(info), std::move(factory));
    }

    // EventInIntervalComputer - Presence
    {
        ComputerInfo info("Event Presence",
                          "Check if events exist in intervals",
                          typeid(bool),
                          "bool",
                          RowSelectorType::IntervalBased,
                          typeid(std::shared_ptr<IEventSource>));

        ComputerFactory factory = [](DataSourceVariant const & source,
                                     std::map<std::string, std::string> const &) -> std::unique_ptr<IComputerBase> {
            if (auto eventSrc = std::get_if<std::shared_ptr<IEventSource>>(&source)) {
                auto computer = std::make_unique<EventInIntervalComputer<bool>>(*eventSrc, EventOperation::Presence, (*eventSrc)->getName());
                return std::make_unique<ComputerWrapper<bool>>(std::move(computer));
            }
            return nullptr;
        };

        registerComputer(std::move(info), std::move(factory));
    }

    // EventInIntervalComputer - Count
    {
        ComputerInfo info("Event Count",
                          "Count events in intervals",
                          typeid(int),
                          "int",
                          RowSelectorType::IntervalBased,
                          typeid(std::shared_ptr<IEventSource>));

        ComputerFactory factory = [](DataSourceVariant const & source,
                                     std::map<std::string, std::string> const &) -> std::unique_ptr<IComputerBase> {
            if (auto eventSrc = std::get_if<std::shared_ptr<IEventSource>>(&source)) {
                auto computer = std::make_unique<EventInIntervalComputer<int>>(*eventSrc, EventOperation::Count, (*eventSrc)->getName());
                return std::make_unique<ComputerWrapper<int>>(std::move(computer));
            }
            return nullptr;
        };

        registerComputer(std::move(info), std::move(factory));
    }

    // IntervalPropertyComputer - Start
    {
        ComputerInfo info("Interval Start",
                          "Get the start time of intervals",
                          typeid(double),
                          "double",
                          RowSelectorType::IntervalBased,
                          typeid(std::shared_ptr<IIntervalSource>));

        ComputerFactory factory = [](DataSourceVariant const & source,
                                     std::map<std::string, std::string> const &) -> std::unique_ptr<IComputerBase> {
            if (auto intervalSrc = std::get_if<std::shared_ptr<IIntervalSource>>(&source)) {
                auto computer = std::make_unique<IntervalPropertyComputer<double>>(*intervalSrc, IntervalProperty::Start, (*intervalSrc)->getName());
                return std::make_unique<ComputerWrapper<double>>(std::move(computer));
            }
            return nullptr;
        };

        registerComputer(std::move(info), std::move(factory));
    }

    // IntervalPropertyComputer - End
    {
        ComputerInfo info("Interval End",
                          "Get the end time of intervals",
                          typeid(double),
                          "double",
                          RowSelectorType::IntervalBased,
                          typeid(std::shared_ptr<IIntervalSource>));

        ComputerFactory factory = [](DataSourceVariant const & source,
                                     std::map<std::string, std::string> const &) -> std::unique_ptr<IComputerBase> {
            if (auto intervalSrc = std::get_if<std::shared_ptr<IIntervalSource>>(&source)) {
                auto computer = std::make_unique<IntervalPropertyComputer<double>>(*intervalSrc, IntervalProperty::End, (*intervalSrc)->getName());
                return std::make_unique<ComputerWrapper<double>>(std::move(computer));
            }
            return nullptr;
        };

        registerComputer(std::move(info), std::move(factory));
    }

    // IntervalPropertyComputer - Duration
    {
        ComputerInfo info("Interval Duration",
                          "Get the duration of intervals",
                          typeid(double),
                          "double",
                          RowSelectorType::IntervalBased,
                          typeid(std::shared_ptr<IIntervalSource>));

        ComputerFactory factory = [](DataSourceVariant const & source,
                                     std::map<std::string, std::string> const &) -> std::unique_ptr<IComputerBase> {
            if (auto intervalSrc = std::get_if<std::shared_ptr<IIntervalSource>>(&source)) {
                auto computer = std::make_unique<IntervalPropertyComputer<double>>(*intervalSrc, IntervalProperty::Duration, (*intervalSrc)->getName());
                return std::make_unique<ComputerWrapper<double>>(std::move(computer));
            }
            return nullptr;
        };

        registerComputer(std::move(info), std::move(factory));
    }

    // EventInIntervalComputer - Gather with parameter
    {
        // Create parameter descriptors
        std::vector<std::unique_ptr<IParameterDescriptor>> paramDescriptors;
        paramDescriptors.push_back(std::make_unique<EnumParameterDescriptor>(
                "mode", "Gathering mode for event times",
                std::vector<std::string>{"absolute", "centered"},
                "absolute", true));

        ComputerInfo info("Event Gather",
                          "Gather event times within intervals",
                          typeid(std::vector<float>),
                          "std::vector<float>",
                          typeid(float),
                          "float",
                          RowSelectorType::IntervalBased,
                          typeid(std::shared_ptr<IEventSource>),
                          std::move(paramDescriptors));

        ComputerFactory factory = [](DataSourceVariant const & source,
                                     std::map<std::string, std::string> const & parameters) -> std::unique_ptr<IComputerBase> {
            if (auto eventSrc = std::get_if<std::shared_ptr<IEventSource>>(&source)) {
                // Parse the mode parameter
                EventOperation operation = EventOperation::Gather;// default
                auto mode_it = parameters.find("mode");
                if (mode_it != parameters.end()) {
                    if (mode_it->second == "centered") {
                        operation = EventOperation::Gather_Center;
                    } else {
                        operation = EventOperation::Gather;
                    }
                }

                auto computer = std::make_unique<EventInIntervalComputer<std::vector<float>>>(
                        *eventSrc, operation, (*eventSrc)->getName());
                return std::make_unique<ComputerWrapper<std::vector<float>>>(std::move(computer));
            }
            return nullptr;
        };

        registerComputer(std::move(info), std::move(factory));
    }

    // TimestampValueComputer - Extract values at specific timestamps
    {
        ComputerInfo info("Timestamp Value",
                          "Extract analog signal values at specific timestamps",
                          typeid(double),
                          "double",
                          RowSelectorType::Timestamp,
                          typeid(std::shared_ptr<IAnalogSource>));

        ComputerFactory factory = [](DataSourceVariant const & source,
                                     std::map<std::string, std::string> const &) -> std::unique_ptr<IComputerBase> {
            if (auto analogSrc = std::get_if<std::shared_ptr<IAnalogSource>>(&source)) {
                auto computer = std::make_unique<TimestampValueComputer>(*analogSrc);
                return std::make_unique<ComputerWrapper<double>>(std::move(computer));
            }
            return nullptr;
        };

        registerComputer(std::move(info), std::move(factory));
    }

    // AnalogTimestampOffsetsMultiComputer - multi-output sampling at offsets
    {
        std::vector<std::unique_ptr<IParameterDescriptor>> paramDescriptors;
        // Offsets provided as comma-separated integers, e.g., "-2,-1,0,1"
        // Use a simple text parameter via IParameterDescriptor base replacement: reuse IntParameterDescriptor for a hint?
        // We'll not enforce via UI here; consumers pass string list in parameters["offsets"].

        ComputerInfo info("Analog Timestamp Offsets",
                          "Sample analog values at specified integer offsets from each timestamp",
                          typeid(double),
                          "double",
                          RowSelectorType::Timestamp,
                          typeid(std::shared_ptr<IAnalogSource>),
                          std::move(paramDescriptors));
        info.isMultiOutput = true;
        info.makeOutputSuffixes = [](std::map<std::string, std::string> const & parameters) {
            std::vector<std::string> suffixes;
            auto it = parameters.find("offsets");
            if (it == parameters.end() || it->second.empty()) {
                // default single output at t+0
                suffixes.emplace_back(".t+0");
                return suffixes;
            }
            std::string const & csv = it->second;
            size_t pos = 0;
            while (pos < csv.size()) {
                size_t next = csv.find(',', pos);
                std::string token = csv.substr(pos, next == std::string::npos ? std::string::npos : next - pos);
                // trim spaces
                size_t beg = token.find_first_not_of(" \t");
                size_t end = token.find_last_not_of(" \t");
                if (beg != std::string::npos) token = token.substr(beg, end - beg + 1);
                int off = 0;
                try {
                    off = std::stoi(token);
                } catch (...) { off = 0; }
                if (off == 0) suffixes.emplace_back(".t+0");
                else if (off > 0)
                    suffixes.emplace_back(".t+" + std::to_string(off));
                else
                    suffixes.emplace_back(".t" + std::to_string(off));
                if (next == std::string::npos) break;
                pos = next + 1;
            }
            if (suffixes.empty()) suffixes.emplace_back(".t+0");
            return suffixes;
        };

        MultiComputerFactory factory = [](DataSourceVariant const & source,
                                          std::map<std::string, std::string> const & parameters) -> std::unique_ptr<IComputerBase> {
            if (auto analogSrc = std::get_if<std::shared_ptr<IAnalogSource>>(&source)) {
                // parse offsets
                std::vector<int> offsets;
                auto it = parameters.find("offsets");
                if (it != parameters.end()) {
                    std::string const & csv = it->second;
                    size_t pos = 0;
                    while (pos < csv.size()) {
                        size_t next = csv.find(',', pos);
                        std::string token = csv.substr(pos, next == std::string::npos ? std::string::npos : next - pos);
                        size_t beg = token.find_first_not_of(" \t");
                        size_t end = token.find_last_not_of(" \t");
                        if (beg != std::string::npos) token = token.substr(beg, end - beg + 1);
                        try {
                            offsets.push_back(std::stoi(token));
                        } catch (...) { offsets.push_back(0); }
                        if (next == std::string::npos) break;
                        pos = next + 1;
                    }
                }
                if (offsets.empty()) offsets.push_back(0);
                auto comp = std::make_unique<AnalogTimestampOffsetsMultiComputer>(*analogSrc, (*analogSrc)->getName(), offsets);
                return std::make_unique<MultiComputerWrapper<double>>(std::move(comp));
            }
            return nullptr;
        };

        registerMultiComputer(std::move(info), std::move(factory));
    }

    // TimestampInIntervalComputer - bool for timestamps inside digital intervals
    {
        ComputerInfo info("Timestamp In Interval",
                          "Returns true if timestamp lies within any digital interval",
                          typeid(bool),
                          "bool",
                          RowSelectorType::Timestamp,
                          typeid(std::shared_ptr<IIntervalSource>));

        ComputerFactory factory = [](DataSourceVariant const & source,
                                     std::map<std::string, std::string> const &) -> std::unique_ptr<IComputerBase> {
            if (auto intervalSrc = std::get_if<std::shared_ptr<IIntervalSource>>(&source)) {
                auto comp = std::make_unique<TimestampInIntervalComputer>(*intervalSrc, (*intervalSrc)->getName());
                return std::make_unique<ComputerWrapper<bool>>(std::move(comp));
            }
            return nullptr;
        };

        registerComputer(std::move(info), std::move(factory));
    }

    // LineSamplingMultiComputer - sample x/y at equally spaced segments along line
    {
        std::vector<std::unique_ptr<IParameterDescriptor>> paramDescriptors;
        paramDescriptors.push_back(std::make_unique<IntParameterDescriptor>(
                "segments", "Number of equal segments to divide the line into (generates segments+1 sample points)", 2, 1, 1000, true));

        ComputerInfo info("Line Sample XY",
                          "Sample line x and y at equally spaced positions",
                          typeid(double),
                          "double",
                          typeid(double),
                          "double",
                          RowSelectorType::Timestamp,
                          typeid(std::shared_ptr<ILineSource>),
                          std::move(paramDescriptors));
        info.isMultiOutput = true;
        info.makeOutputSuffixes = [](std::map<std::string, std::string> const & parameters) {
            int segments = 2;
            auto it = parameters.find("segments");
            if (it != parameters.end()) {
                segments = std::max(1, std::stoi(it->second));
            }
            std::vector<std::string> suffixes;
            suffixes.reserve(static_cast<size_t>((segments + 1) * 2));
            for (int i = 0; i <= segments; ++i) {
                double frac = static_cast<double>(i) / static_cast<double>(segments);
                char buf[32];
                std::snprintf(buf, sizeof(buf), "@%.3f", frac);
                suffixes.emplace_back(std::string{".x"} + buf);
                suffixes.emplace_back(std::string{".y"} + buf);
            }
            return suffixes;
        };

        MultiComputerFactory factory = [](DataSourceVariant const & source,
                                          std::map<std::string, std::string> const & parameters) -> std::unique_ptr<IComputerBase> {
            if (auto lineSrc = std::get_if<std::shared_ptr<ILineSource>>(&source)) {
                int segments = 2;
                auto it = parameters.find("segments");
                if (it != parameters.end()) {
                    segments = std::max(1, std::stoi(it->second));
                }
                auto comp = std::make_unique<LineSamplingMultiComputer>(*lineSrc, (*lineSrc)->getName(), (*lineSrc)->getTimeFrame(), segments);
                return std::make_unique<MultiComputerWrapper<double>>(std::move(comp));
            }
            return nullptr;
        };

        registerMultiComputer(std::move(info), std::move(factory));
    }

    // IntervalOverlapComputer - AssignID operation
    {
        ComputerInfo info("Interval Overlap Assign ID",
                          "Find the ID of the column interval that overlaps with each row interval",
                          typeid(int64_t),
                          "int64_t",
                          RowSelectorType::IntervalBased,
                          typeid(std::shared_ptr<IIntervalSource>));

        ComputerFactory factory = [](DataSourceVariant const & source,
                                     std::map<std::string, std::string> const &) -> std::unique_ptr<IComputerBase> {
            if (auto intervalSrc = std::get_if<std::shared_ptr<IIntervalSource>>(&source)) {
                auto computer = std::make_unique<IntervalOverlapComputer<int64_t>>(
                        *intervalSrc, IntervalOverlapOperation::AssignID, (*intervalSrc)->getName());
                return std::make_unique<ComputerWrapper<int64_t>>(std::move(computer));
            }
            return nullptr;
        };

        registerComputer(std::move(info), std::move(factory));
    }

    // IntervalOverlapComputer - CountOverlaps operation
    {
        ComputerInfo info("Interval Overlap Count",
                          "Count the number of column intervals that overlap with each row interval",
                          typeid(int64_t),
                          "int64_t",
                          RowSelectorType::IntervalBased,
                          typeid(std::shared_ptr<IIntervalSource>));

        ComputerFactory factory = [](DataSourceVariant const & source,
                                     std::map<std::string, std::string> const &) -> std::unique_ptr<IComputerBase> {
            if (auto intervalSrc = std::get_if<std::shared_ptr<IIntervalSource>>(&source)) {
                auto computer = std::make_unique<IntervalOverlapComputer<int64_t>>(
                        *intervalSrc, IntervalOverlapOperation::CountOverlaps, (*intervalSrc)->getName());
                return std::make_unique<ComputerWrapper<int64_t>>(std::move(computer));
            }
            return nullptr;
        };

        registerComputer(std::move(info), std::move(factory));
    }

    // IntervalOverlapComputer - AssignID_Start operation
    {
        ComputerInfo info("Interval Overlap Assign Start",
                          "Find the start index of the column interval that overlaps with each row interval",
                          typeid(int64_t),
                          "int64_t",
                          RowSelectorType::IntervalBased,
                          typeid(std::shared_ptr<IIntervalSource>));

        ComputerFactory factory = [](DataSourceVariant const & source,
                                     std::map<std::string, std::string> const &) -> std::unique_ptr<IComputerBase> {
            if (auto intervalSrc = std::get_if<std::shared_ptr<IIntervalSource>>(&source)) {
                auto computer = std::make_unique<IntervalOverlapComputer<int64_t>>(
                        *intervalSrc, IntervalOverlapOperation::AssignID_Start, (*intervalSrc)->getName());
                return std::make_unique<ComputerWrapper<int64_t>>(std::move(computer));
            }
            return nullptr;
        };

        registerComputer(std::move(info), std::move(factory));
    }

    // IntervalOverlapComputer - AssignID_End operation
    {
        ComputerInfo info("Interval Overlap Assign End",
                          "Find the end index of the column interval that overlaps with each row interval",
                          typeid(int64_t),
                          "int64_t",
                          RowSelectorType::IntervalBased,
                          typeid(std::shared_ptr<IIntervalSource>));

        ComputerFactory factory = [](DataSourceVariant const & source,
                                     std::map<std::string, std::string> const &) -> std::unique_ptr<IComputerBase> {
            if (auto intervalSrc = std::get_if<std::shared_ptr<IIntervalSource>>(&source)) {
                auto computer = std::make_unique<IntervalOverlapComputer<int64_t>>(
                        *intervalSrc, IntervalOverlapOperation::AssignID_End, (*intervalSrc)->getName());
                return std::make_unique<ComputerWrapper<int64_t>>(std::move(computer));
            }
            return nullptr;
        };

        registerComputer(std::move(info), std::move(factory));
    }

    // AnalogSliceGathererComputer - Gather analog data slices within intervals
    {
        ComputerInfo info("Analog Slice Gatherer",
                          "Gather analog data slices within intervals as vectors",
                          typeid(std::vector<double>),
                          "std::vector<double>",
                          typeid(double),
                          "double",
                          RowSelectorType::IntervalBased,
                          typeid(std::shared_ptr<IAnalogSource>));

        ComputerFactory factory = [](DataSourceVariant const & source,
                                     std::map<std::string, std::string> const &) -> std::unique_ptr<IComputerBase> {
            if (auto analogSrc = std::get_if<std::shared_ptr<IAnalogSource>>(&source)) {
                auto computer = std::make_unique<AnalogSliceGathererComputer<std::vector<double>>>(*analogSrc, (*analogSrc)->getName());
                return std::make_unique<ComputerWrapper<std::vector<double>>>(std::move(computer));
            }
            return nullptr;
        };

        registerComputer(std::move(info), std::move(factory));
    }

    // AnalogSliceGathererComputer - Float version
    {
        ComputerInfo info("Analog Slice Gatherer Float",
                          "Gather analog data slices within intervals as vectors of floats",
                          typeid(std::vector<float>),
                          "std::vector<float>",
                          typeid(float),
                          "float",
                          RowSelectorType::IntervalBased,
                          typeid(std::shared_ptr<IAnalogSource>));

        ComputerFactory factory = [](DataSourceVariant const & source,
                                     std::map<std::string, std::string> const &) -> std::unique_ptr<IComputerBase> {
            if (auto analogSrc = std::get_if<std::shared_ptr<IAnalogSource>>(&source)) {
                auto computer = std::make_unique<AnalogSliceGathererComputer<std::vector<float>>>(*analogSrc, (*analogSrc)->getName());
                return std::make_unique<ComputerWrapper<std::vector<float>>>(std::move(computer));
            }
            return nullptr;
        };

        registerComputer(std::move(info), std::move(factory));
    }

    //std::cout << "Finished registering built-in computers." << std::endl;
}

void ComputerRegistry::registerBuiltInAdapters() {
    //std::cout << "Registering built-in adapters..." << std::endl;

    // PointComponentAdapter - X Component
    {
        AdapterInfo info("Point X Component",
                         "Extract X component from PointData as analog source",
                         typeid(PointData),
                         typeid(std::shared_ptr<IAnalogSource>));

        AdapterFactory factory = [](std::shared_ptr<void> const & sourceData,
                                    std::shared_ptr<TimeFrame> const & timeFrame,
                                    std::string const & name,
                                    std::map<std::string, std::string> const &) -> DataSourceVariant {
            if (auto pointData = std::static_pointer_cast<PointData>(sourceData)) {
                auto adapter = std::make_shared<PointComponentAdapter>(
                        pointData,
                        PointComponentAdapter::Component::X,
                        timeFrame,
                        name + "_X");
                return DataSourceVariant{std::static_pointer_cast<IAnalogSource>(adapter)};
            }
            return DataSourceVariant{};
        };

        registerAdapter(std::move(info), std::move(factory));
    }

    // PointComponentAdapter - Y Component
    {
        AdapterInfo info("Point Y Component",
                         "Extract Y component from PointData as analog source",
                         typeid(PointData),
                         typeid(std::shared_ptr<IAnalogSource>));

        AdapterFactory factory = [](std::shared_ptr<void> const & sourceData,
                                    std::shared_ptr<TimeFrame> const & timeFrame,
                                    std::string const & name,
                                    std::map<std::string, std::string> const &) -> DataSourceVariant {
            if (auto pointData = std::static_pointer_cast<PointData>(sourceData)) {
                auto adapter = std::make_shared<PointComponentAdapter>(
                        pointData,
                        PointComponentAdapter::Component::Y,
                        timeFrame,
                        name + "_Y");
                return DataSourceVariant{std::static_pointer_cast<IAnalogSource>(adapter)};
            }
            return DataSourceVariant{};
        };

        registerAdapter(std::move(info), std::move(factory));
    }

    // LineDataAdapter - LineData -> ILineSource
    {
        AdapterInfo info("Line Data",
                         "Expose LineData as ILineSource",
                         typeid(LineData),
                         typeid(std::shared_ptr<ILineSource>));

        AdapterFactory factory = [](std::shared_ptr<void> const & sourceData,
                                    std::shared_ptr<TimeFrame> const & timeFrame,
                                    std::string const & name,
                                    std::map<std::string, std::string> const &) -> DataSourceVariant {
            if (auto ld = std::static_pointer_cast<LineData>(sourceData)) {
                auto adapter = std::make_shared<LineDataAdapter>(ld, timeFrame, name);
                return DataSourceVariant{std::static_pointer_cast<ILineSource>(adapter)};
            }
            return DataSourceVariant{};
        };

        registerAdapter(std::move(info), std::move(factory));
    }

    //std::cout << "Finished registering built-in adapters." << std::endl;
}
