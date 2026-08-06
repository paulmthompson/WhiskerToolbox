/**
 * @file Spike2LoaderRegistration.hpp
 * @brief Application-level registration hook for the Python-backed Spike2 loader.
 */

#ifndef WHISKERTOOLBOX_SPIKE2_LOADER_REGISTRATION_HPP
#define WHISKERTOOLBOX_SPIKE2_LOADER_REGISTRATION_HPP

/**
 * @brief Register Python-backed Spike2 loaders when the optional target is linked.
 */
void registerSpike2PythonLoaderIfAvailable();

/**
 * @brief Register the JSON `python` environment configurator used by DataManager.
 */
void registerJsonPythonEnvironmentConfigurator();

#endif// WHISKERTOOLBOX_SPIKE2_LOADER_REGISTRATION_HPP
