/**
 * @file Spike2LoaderRegistration.cpp
 * @brief Application-level registration hook for the Python-backed Spike2 loader.
 */

#include "Spike2LoaderRegistration.hpp"

#include "PythonRuntimeService.hpp"

#include "DataManager/DataManager.hpp"
#include "IO/core/LoaderRegistry.hpp"
#include "JsonPythonEnvironment.hpp"

#include <memory>
#include <string>

#ifdef ENABLE_SPIKE2_SONPY
#include "IO/formats/Spike2/Spike2PythonFormatLoader.hpp"
#endif

void registerSpike2PythonLoaderIfAvailable() {
#ifdef ENABLE_SPIKE2_SONPY
    static bool registered = false;
    if (registered) {
        return;
    }

    LoaderRegistry::getInstance().registerLoader(
            std::make_unique<Spike2PythonFormatLoader>(PythonRuntimeService::instance().engine()));
    registered = true;
#endif
}

void registerJsonPythonEnvironmentConfigurator() {
    setJsonPythonEnvironmentConfigurator([](nlohmann::json const & config, std::string & error_message) {
        return configurePythonEnvironmentFromJson(PythonRuntimeService::instance().engine(), config, error_message);
    });
}
