#ifndef PROCESSING_OPTIONS_HPP
#define PROCESSING_OPTIONS_HPP

/**
 * @brief Options structure for linear contrast/brightness transformation
 */
struct ContrastOptions {
    bool active{false};           ///< Whether the contrast filter is active
    double alpha{1.0};           ///< Contrast multiplier (alpha parameter)
    int beta{0};                 ///< Brightness additive (beta parameter)
};

/**
 * @brief Options structure for gamma correction
 */
struct GammaOptions {
    bool active{false};          ///< Whether the gamma filter is active
    double gamma{1.0};           ///< Gamma correction value
};

/**
 * @brief Options structure for image sharpening
 */
struct SharpenOptions {
    bool active{false};          ///< Whether the sharpen filter is active
    double sigma{3.0};           ///< Sigma parameter for sharpening
};

/**
 * @brief Options structure for CLAHE (Contrast Limited Adaptive Histogram Equalization)
 */
struct ClaheOptions {
    bool active{false};          ///< Whether the CLAHE filter is active
    int grid_size{8};            ///< Grid size for CLAHE
    double clip_limit{2.0};      ///< Clip limit for CLAHE
};

/**
 * @brief Options structure for bilateral filtering
 */
struct BilateralOptions {
    bool active{false};          ///< Whether the bilateral filter is active
    int diameter{5};             ///< Diameter of bilateral filter
    double sigma_color{20.0};    ///< Color sigma for bilateral filter
    double sigma_spatial{20.0};  ///< Spatial sigma for bilateral filter
};

/**
 * @brief Options structure for median filtering
 */
struct MedianOptions {
    bool active{false};          ///< Whether the median filter is active
    int kernel_size{5};          ///< Kernel size for median filter (must be odd and >= 3)
};

#endif // PROCESSING_OPTIONS_HPP 
