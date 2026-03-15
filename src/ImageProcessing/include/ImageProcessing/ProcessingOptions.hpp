#ifndef IMAGE_PROCESSING_OPTIONS_HPP
#define IMAGE_PROCESSING_OPTIONS_HPP

#include "CoreGeometry/ImageSize.hpp"

#include <cstdint>
#include <vector>

/**
 * @brief Options structure for linear contrast/brightness transformation
 *
 * The `active` state is managed externally at the processing chain level,
 * not within this struct. This struct only holds the filter parameters.
 */
struct ContrastOptions {
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

        display_min = -static_cast<double>(beta) / alpha;
        display_max = (255.0 - static_cast<double>(beta)) / alpha;
    }
};

/**
 * @brief Options structure for gamma correction
 */
struct GammaOptions {
    double gamma{1.0}; ///< Gamma correction value
};

/**
 * @brief Options structure for image sharpening
 */
struct SharpenOptions {
    double sigma{3.0}; ///< Sigma parameter for sharpening
};

/**
 * @brief Options structure for CLAHE (Contrast Limited Adaptive Histogram Equalization)
 */
struct ClaheOptions {
    int grid_size{8};      ///< Grid size for CLAHE
    double clip_limit{2.0};///< Clip limit for CLAHE
};

/**
 * @brief Options structure for bilateral filtering
 */
struct BilateralOptions {
    int diameter{5};           ///< Diameter of bilateral filter
    double sigma_color{20.0};  ///< Color sigma for bilateral filter
    double sigma_spatial{20.0};///< Spatial sigma for bilateral filter
};

/**
 * @brief Options structure for median filtering
 */
struct MedianOptions {
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
 * @brief UI-editable parameters for the magic eraser tool
 *
 * These are the parameters that can be configured by the user via the UI.
 * Runtime state (mask, drawing mode, image size) is stored separately
 * in MagicEraserState.
 */
struct MagicEraserParams {
    int brush_size{10};        ///< Size of the eraser brush (1-100 pixels)
    int median_filter_size{25};///< Size of the median filter kernel (3-101, must be odd)
};

/**
 * @brief Runtime state for the magic eraser tool
 *
 * This state is not user-configurable and is managed by the processing chain.
 * It includes the drawing mask, drawing mode toggle, and image dimensions.
 */
struct MagicEraserState {
    bool drawing_mode{false};  ///< Whether currently in drawing mode
    std::vector<uint8_t> mask; ///< Mask of pixels to be replaced (empty = no replacement)
    ImageSize image_size;      ///< Size of the image the mask corresponds to
};

/**
 * @brief Combined magic eraser options for use by ImageProcessing functions
 *
 * This struct combines MagicEraserParams and MagicEraserState for backward
 * compatibility with existing ImageProcessing functions that accept
 * a single options struct.
 */
struct MagicEraserOptions {
    int brush_size{10};        ///< Size of the eraser brush (1-100 pixels)
    int median_filter_size{25};///< Size of the median filter kernel (3-101, must be odd)
    bool drawing_mode{false};  ///< Whether currently in drawing mode
    std::vector<uint8_t> mask; ///< Mask of pixels to be replaced (empty = no replacement)
    ImageSize image_size;      ///< Size of the image the mask corresponds to

    /// Construct from separate params and state
    static MagicEraserOptions fromParamsAndState(MagicEraserParams const & params, MagicEraserState const & state) {
        return MagicEraserOptions{
                .brush_size = params.brush_size,
                .median_filter_size = params.median_filter_size,
                .drawing_mode = state.drawing_mode,
                .mask = state.mask,
                .image_size = state.image_size};
    }
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
    ColormapType colormap{ColormapType::None};///< Selected colormap type
    double alpha{1.0};                        ///< Alpha blending with original image (0.0-1.0)
    bool normalize{true};                     ///< Whether to normalize image values before applying colormap
};

#endif// IMAGE_PROCESSING_OPTIONS_HPP
