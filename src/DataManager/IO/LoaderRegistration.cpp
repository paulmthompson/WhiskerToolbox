#include "LoaderRegistration.hpp"

#include "LoaderRegistry.hpp"
#include "loaders/CSVLoader.hpp"

// Conditional includes based on compile-time options
#ifdef ENABLE_CAPNPROTO
#include "CapnProto/CapnProtoFormatLoader.hpp"
#endif

#ifdef ENABLE_HDF5
#include "HDF5/HDF5FormatLoader.hpp"
#endif

#ifdef ENABLE_OPENCV
#include "OpenCV/OpenCVFormatLoader.hpp"
#endif

#include <iostream>
#include <memory>

void registerAllLoaders() {
    std::cout << "LoaderRegistration: Registering all loaders..." << std::endl;
    
    registerInternalLoaders();
    registerExternalLoaders();
    
    std::cout << "LoaderRegistration: All loaders registered." << std::endl;
}

void registerInternalLoaders() {
    [[maybe_unused]] LoaderRegistry& registry = LoaderRegistry::getInstance();
    
    // Register CSV loader (always available)
    std::cout << "LoaderRegistration: Registering CSV loader..." << std::endl;
    registry.registerLoader(std::make_unique<CSVLoader>());
}

void registerExternalLoaders() {
    LoaderRegistry& registry = LoaderRegistry::getInstance();
    
#ifdef ENABLE_CAPNPROTO
    // Register CapnProto loader if available
    std::cout << "LoaderRegistration: Registering CapnProto loader..." << std::endl;
    registry.registerLoader(std::make_unique<CapnProtoFormatLoader>());
#else
    std::cout << "LoaderRegistration: CapnProto loader not available (ENABLE_CAPNPROTO not defined)" << std::endl;
#endif

#ifdef ENABLE_HDF5
    // Register HDF5 loader if available
    std::cout << "LoaderRegistration: Registering HDF5 loader..." << std::endl;
    registry.registerLoader(std::make_unique<HDF5FormatLoader>());
#else
    std::cout << "LoaderRegistration: HDF5 loader not available (ENABLE_HDF5 not defined)" << std::endl;
#endif

#ifdef ENABLE_OPENCV
    // Register OpenCV loader if available
    std::cout << "LoaderRegistration: Registering OpenCV loader..." << std::endl;
    registry.registerLoader(std::make_unique<OpenCVFormatLoader>());
#else
    std::cout << "LoaderRegistration: OpenCV loader not available (ENABLE_OPENCV not defined)" << std::endl;
#endif

    // Future external loaders can be added here with similar conditional compilation
}
