#include "OpenCVInit.hpp"

// Forward declaration to force linking
extern "C" void ensure_opencv_loader_registration();

void initializeOpenCVLoader() {
    // Call the function to ensure the object file containing the loader
    // registration is linked into the final executable
    ::ensure_opencv_loader_registration();
}
