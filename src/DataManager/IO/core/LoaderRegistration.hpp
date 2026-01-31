#ifndef LOADER_REGISTRATION_HPP
#define LOADER_REGISTRATION_HPP

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
void registerAllLoaders();

/**
 * @brief Register internal loaders (no external dependencies)
 */
void registerInternalLoaders();

/**
 * @brief Register external loaders (with dependencies)
 */
void registerExternalLoaders();

#endif // LOADER_REGISTRATION_HPP
