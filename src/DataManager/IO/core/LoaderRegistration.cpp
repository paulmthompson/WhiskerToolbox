#include "LoaderRegistration.hpp"

#include "LoaderRegistry.hpp"
#include "formats/CSV/CSVLoader.hpp"
#include "formats/Analog/AnalogLoader.hpp"
#include "formats/Digital/DigitalEventLoader.hpp"
#include "formats/Digital/DigitalIntervalLoader.hpp"

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
    
    // Register CSV loader for LineData (always available)
   // std::cout << "LoaderRegistration: Registering CSV loader..." << std::endl;
    registry.registerLoader(std::make_unique<CSVLoader>());
    
    // Register Analog loader for AnalogTimeSeries (binary and CSV formats)
   // std::cout << "LoaderRegistration: Registering Analog loader..." << std::endl;
    registry.registerLoader(std::make_unique<AnalogLoader>());
    
    // Register Digital Event loader for DigitalEventSeries (uint16 and CSV formats)
   // std::cout << "LoaderRegistration: Registering DigitalEvent loader..." << std::endl;
    registry.registerLoader(std::make_unique<DigitalEventLoader>());
    
    // Register Digital Interval loader for DigitalIntervalSeries (uint16, CSV, multi_column_binary formats)
   // std::cout << "LoaderRegistration: Registering DigitalInterval loader..." << std::endl;
    registry.registerLoader(std::make_unique<DigitalIntervalLoader>());
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
