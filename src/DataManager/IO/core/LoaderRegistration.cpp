#include "LoaderRegistration.hpp"

#include "LoaderRegistry.hpp"

// Format-centric loaders (preferred architecture)
#include "formats/CSV/CSVLoader.hpp"
#include "formats/Binary/BinaryFormatLoader.hpp"

// DEPRECATED: Data-centric loaders (kept temporarily for backward compatibility)
// These will be removed once all DataManager.cpp code uses format-centric loaders
#include "formats/Analog/AnalogLoader.hpp"
#include "formats/Digital/DigitalEventLoader.hpp"
#include "formats/Digital/DigitalIntervalLoader.hpp"
#include "formats/Points/PointLoader.hpp"

// Conditional includes based on compile-time options
#ifdef ENABLE_CAPNPROTO
#include "formats/CapnProto/CapnProtoFormatLoader.hpp"
#endif

#ifdef ENABLE_HDF5
#include "formats/HDF5/HDF5FormatLoader.hpp"
#endif

#ifdef ENABLE_OPENCV
#include "formats/OpenCV/OpenCVFormatLoader.hpp"
#endif

#include <iostream>
#include <memory>

void registerAllLoaders() {
   // std::cout << "LoaderRegistration: Registering all loaders..." << std::endl;
    
    registerInternalLoaders();
    registerExternalLoaders();
    
   // std::cout << "LoaderRegistration: All loaders registered." << std::endl;
}

void registerInternalLoaders() {
    LoaderRegistry& registry = LoaderRegistry::getInstance();
    
    // =========================================================================
    // Format-centric loaders (preferred - these handle multiple data types)
    // =========================================================================
    
    // CSVFormatLoader: Line, Points, Analog, DigitalEvent, DigitalInterval
    // Supports batch loading for DigitalEvent (multi-series) and Points (DLC multi-bodypart)
    registry.registerLoader(std::make_unique<CSVLoader>());
    
    // BinaryFormatLoader: Analog, DigitalEvent, DigitalInterval
    // Supports batch loading for multi-channel binary files
    registry.registerLoader(std::make_unique<BinaryFormatLoader>());
    
    // =========================================================================
    // DEPRECATED: Data-centric loaders (kept for backward compatibility)
    // These are registered AFTER format-centric loaders so format-centric
    // loaders are tried first. They will be removed in a future cleanup.
    // =========================================================================
    
    // Note: These are now redundant with the format-centric loaders above,
    // but kept temporarily to ensure nothing breaks during migration.
    // They can be removed once DataManager.cpp fully uses tryLoadBatch().
    
    registry.registerLoader(std::make_unique<AnalogLoader>());
    registry.registerLoader(std::make_unique<DigitalEventLoader>());
    registry.registerLoader(std::make_unique<DigitalIntervalLoader>());
    registry.registerLoader(std::make_unique<PointLoader>());
}

void registerExternalLoaders() {

    LoaderRegistry& registry = LoaderRegistry::getInstance();
    
#ifdef ENABLE_CAPNPROTO
    // Register CapnProto loader if available
    //std::cout << "LoaderRegistration: Registering CapnProto loader..." << std::endl;
    registry.registerLoader(std::make_unique<CapnProtoFormatLoader>());
#else
    std::cout << "LoaderRegistration: CapnProto loader not available (ENABLE_CAPNPROTO not defined)" << std::endl;
#endif

#ifdef ENABLE_HDF5
    // Register HDF5 loader if available
    //std::cout << "LoaderRegistration: Registering HDF5 loader..." << std::endl;
    registry.registerLoader(std::make_unique<HDF5FormatLoader>());
#else
    std::cout << "LoaderRegistration: HDF5 loader not available (ENABLE_HDF5 not defined)" << std::endl;
#endif

#ifdef ENABLE_OPENCV
    // Register OpenCV loader if available
   // std::cout << "LoaderRegistration: Registering OpenCV loader..." << std::endl;
    registry.registerLoader(std::make_unique<OpenCVFormatLoader>());
#else
    std::cout << "LoaderRegistration: OpenCV loader not available (ENABLE_OPENCV not defined)" << std::endl;
#endif

    // Future external loaders can be added here with similar conditional compilation
}
