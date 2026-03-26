#ifndef LOADER_REGISTRATION_HPP
#define LOADER_REGISTRATION_HPP

#include "datamanagerio_export.h"

class LoaderRegistry;

/**
 * @brief Initialize and register all available loaders
 * 
 * This function should be called once at application startup to register
 * all internal and external loaders with the LoaderRegistry.
 * 
 * The registration happens at compile time based on available dependencies:
 * - CSV loader (always available - internal)
 * - CapnProto loader (if ENABLE_CAPNPROTO is set)
 * - Future loaders can be added here
 */
DATAMANAGERIO_EXPORT void registerAllLoaders();

/**
 * @brief Register internal loaders (no external dependencies)
 */
DATAMANAGERIO_EXPORT void registerInternalLoaders();

/// @overload Register internal loaders into a specific registry (for testing).
DATAMANAGERIO_EXPORT void registerInternalLoaders(LoaderRegistry & registry);

/**
 * @brief Register external loaders (with dependencies)
 */
void registerExternalLoaders();

/// @overload Register external loaders into a specific registry (for testing).
void registerExternalLoaders(LoaderRegistry & registry);

#endif// LOADER_REGISTRATION_HPP
