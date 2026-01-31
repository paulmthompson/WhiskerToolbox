#ifndef DATAMANAGER_IO_OPENCV_INIT_HPP
#define DATAMANAGER_IO_OPENCV_INIT_HPP

/**
 * @brief Initialize OpenCV loader registration
 * 
 * Call this function to ensure the OpenCV loader is registered with the
 * LoaderRegistry. This is needed because static registration might not
 * happen automatically in all build configurations.
 */
void initializeOpenCVLoader();

#endif // DATAMANAGER_IO_OPENCV_INIT_HPP
