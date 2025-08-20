#include "ImageProcessor.hpp"

namespace ImageProcessing {

std::map<std::string, ProcessorFactory>& ProcessorRegistry::getRegistry() {
    static std::map<std::string, ProcessorFactory> registry;
    return registry;
}

void ProcessorRegistry::registerProcessor(std::string const& name, ProcessorFactory factory) {
    getRegistry()[name] = std::move(factory);
}

std::unique_ptr<ImageProcessor> ProcessorRegistry::createProcessor(std::string const& name) {
    auto& registry = getRegistry();
    auto it = registry.find(name);
    if (it != registry.end()) {
        return it->second();
    }
    return nullptr;
}

std::vector<std::string> ProcessorRegistry::getAvailableProcessors() {
    std::vector<std::string> names;
    auto& registry = getRegistry();
    names.reserve(registry.size());
    
    for (auto const& [name, factory] : registry) {
        names.push_back(name);
    }
    
    return names;
}

bool ProcessorRegistry::isProcessorAvailable(std::string const& name) {
    auto& registry = getRegistry();
    return registry.find(name) != registry.end();
}

} // namespace ImageProcessing
