#include "LoaderRegistration.hpp"

#include "LoaderRegistry.hpp"

// Format-centric loaders - the unified approach
// CSVLoader handles: Line, Points, Analog, DigitalEvent, DigitalInterval
// BinaryFormatLoader handles: Analog, DigitalEvent, DigitalInterval
#include "formats/CSV/CSVLoader.hpp"
#include "formats/Binary/BinaryFormatLoader.hpp"

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
    // Format-centric loaders - the unified architecture
    // Each loader handles one file format for all applicable data types
    // =========================================================================
    
    // CSVLoader handles all CSV-based data types:
    // - IODataType::Line: Single/multi-file CSV whisker data
    // - IODataType::Points: Simple CSV or DLC format (with batch loading for multi-bodypart)
    // - IODataType::Analog: Single/two column CSV time series
    // - IODataType::DigitalEvent: Event timestamps (with batch loading for multi-series)
    // - IODataType::DigitalInterval: Start/end column pairs
    registry.registerLoader(std::make_unique<CSVLoader>());
    
    // BinaryFormatLoader handles all binary-based data types:
    // - IODataType::Analog: Multi-channel binary (int16, float32, etc.) with batch loading
    // - IODataType::DigitalEvent: TTL extraction from binary
    // - IODataType::DigitalInterval: TTL extraction from binary
    registry.registerLoader(std::make_unique<BinaryFormatLoader>());
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
