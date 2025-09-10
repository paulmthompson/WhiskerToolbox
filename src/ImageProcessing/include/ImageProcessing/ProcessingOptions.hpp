#ifndef IMAGE_PROCESSING_OPTIONS_HPP
#define IMAGE_PROCESSING_OPTIONS_HPP

#include "CoreGeometry/ImageSize.hpp"

#include <cstdint>
#include <vector>

/**
 * @brief Options structure for linear contrast/brightness transformation
 */
/**
 * @brief Options structure for linear contrast/brightness transformation
 */
struct ContrastOptions {
    bool active{false};///< Whether the contrast filter is active
    double alpha{1.0}; ///< Contrast multiplier (alpha parameter)
    int beta{0};       ///< Brightness additive (beta parameter)

    // New min/max display range parameters
    double display_min{0.0};  ///< Minimum display value (mapped to 0)
    double display_max{255.0};///< Maximum display value (mapped to 255)

    /**
     * @brief Calculate alpha and beta from display min/max values
     */
    void calculateAlphaBetaFromMinMax() {
        if (display_max <= display_min) {
            alpha = 1.0;
            beta = 0;
            return;
        }

        // Linear mapping: output = alpha * input + beta
        // We want: display_min -> 0, display_max -> 255
        // So: 0 = alpha * display_min + beta
        //     255 = alpha * display_max + beta
        // Solving: alpha = 255 / (display_max - display_min)
        //          beta = -alpha * display_min

        alpha = 255.0 / (display_max - display_min);
        beta = static_cast<int>(-alpha * display_min);
    }

    /**
     * @brief Calculate display min/max from current alpha and beta values
     */
    void calculateMinMaxFromAlphaBeta() {
        if (alpha == 0.0) {
            display_min = 0.0;
            display_max = 255.0;
            return;
        }

        // Reverse the calculation:
        // output = alpha * input + beta
        // For output = 0: input = -beta / alpha
        // For output = 255: input = (255 - beta) / alpha

        display_min = -static_cast<double>(beta) / alpha;
        display_max = (255.0 - static_cast<double>(beta)) / alpha;
    }
};

/**
 * @brief Options structure for gamma correction
 */
struct GammaOptions {
    bool active{false};///< Whether the gamma filter is active
    double gamma{1.0}; ///< Gamma correction value
};

/**
 * @brief Options structure for image sharpening
 */
struct SharpenOptions {
    bool active{false};///< Whether the sharpen filter is active
    double sigma{3.0}; ///< Sigma parameter for sharpening
};

/**
 * @brief Options structure for CLAHE (Contrast Limited Adaptive Histogram Equalization)
 */
struct ClaheOptions {
    bool active{false};    ///< Whether the CLAHE filter is active
    int grid_size{8};      ///< Grid size for CLAHE
    double clip_limit{2.0};///< Clip limit for CLAHE
};

/**
 * @brief Options structure for bilateral filtering
 */
struct BilateralOptions {
    bool active{false};        ///< Whether the bilateral filter is active
    int diameter{5};           ///< Diameter of bilateral filter
    double sigma_color{20.0};  ///< Color sigma for bilateral filter
    double sigma_spatial{20.0};///< Spatial sigma for bilateral filter
};

/**
 * @brief Options structure for median filtering
 */
struct MedianOptions {
    bool active{false};///< Whether the median filter is active
    int kernel_size{5};///< Kernel size for median filter (must be odd and >= 3)
};

/**
 * @brief Options for mask dilation/erosion operations
 */
struct MaskDilationOptions {
    bool active{false};     ///< Whether the dilation filter is active
    bool preview{false};    ///< Whether to show preview of dilation
    int grow_size{1};       ///< Size for growing the mask (1-100)
    int shrink_size{1};     ///< Size for shrinking the mask (1-100)
    bool is_grow_mode{true};///< True for grow mode, false for shrink mode
};

/**
 * @brief Options for magic eraser tool operations
 */
struct MagicEraserOptions {
    bool active{false};        ///< Whether the magic eraser is active
    int brush_size{10};        ///< Size of the eraser brush (1-100 pixels)
    int median_filter_size{25};///< Size of the median filter kernel (3-101, must be odd)
    bool drawing_mode{false};  ///< Whether currently in drawing mode
    std::vector<uint8_t> mask; ///< Mask of pixels to be replaced (empty = no replacement)
    ImageSize image_size;
};

/**
 * @brief Available colormap types for grayscale images
 */
enum class ColormapType {
    None,   ///< No colormap (grayscale)
    Jet,    ///< Blue to red colormap
    Hot,    ///< Black-red-yellow-white colormap
    Cool,   ///< Cyan-magenta colormap
    Spring, ///< Magenta-yellow colormap
    Summer, ///< Green-yellow colormap
    Autumn, ///< Red-yellow colormap
    Winter, ///< Blue-cyan colormap
    Rainbow,///< Rainbow colormap
    Ocean,  ///< Dark blue to cyan colormap
    Pink,   ///< Pink colormap
    HSV,    ///< HSV colormap
    Parula, ///< Blue-cyan-yellow colormap
    Viridis,///< Purple-blue-green-yellow colormap
    Plasma, ///< Purple-pink-yellow colormap
    Inferno,///< Black-purple-yellow colormap
    Magma,  ///< Black-purple-pink-yellow colormap
    Turbo,  ///< Blue-cyan-green-yellow-red colormap
    // Single-color channel mappings
    Red,    ///< Black to red single-channel colormap
    Green,  ///< Black to green single-channel colormap
    Blue,   ///< Black to blue single-channel colormap
    Cyan,   ///< Black to cyan single-channel colormap
    Magenta,///< Black to magenta single-channel colormap
    Yellow  ///< Black to yellow single-channel colormap
};

/**
 * @brief Options for colormap application to grayscale images
 */
struct ColormapOptions {
    bool active{false};                       ///< Whether the colormap is active
    ColormapType colormap{ColormapType::None};///< Selected colormap type
    double alpha{1.0};                        ///< Alpha blending with original image (0.0-1.0)
    bool normalize{true};                     ///< Whether to normalize image values before applying colormap
};

#endif// IMAGE_PROCESSING_OPTIONS_HPP
