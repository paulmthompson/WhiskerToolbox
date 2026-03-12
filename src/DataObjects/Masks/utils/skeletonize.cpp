/*

The code below was adapted from scikit-image

Function: skimage/morphology/_skeletonize_various_cy.pyx:_skeletonize_loop
Copyright: 2003-2009 Massachusetts Institute of Technology
           2009-2011 Broad Institute
           2003 Lee Kamentsky
License: BSD-3-Clause
*/

#include "skeletonize.hpp"

#include <array>
#include <cstring>

std::vector<uint8_t> fast_skeletonize(std::vector<uint8_t> const & image, size_t height, size_t width) {

    // Look up table
    static std::array<uint8_t, 256> const lut = {0, 0, 0, 1, 0, 0, 1, 3, 0, 0, 3, 1, 1, 0,
                                                 1, 3, 0, 0, 0, 0, 0, 0, 0, 0, 2, 0, 2, 0,
                                                 3, 0, 3, 3, 0, 0, 0, 0, 0, 0, 0, 0, 3, 0,
                                                 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
                                                 2, 0, 0, 0, 3, 0, 2, 2, 0, 0, 0, 0, 0, 0,
                                                 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
                                                 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 2, 0,
                                                 0, 0, 0, 0, 0, 0, 2, 0, 0, 0, 2, 0, 0, 0,
                                                 3, 0, 0, 0, 0, 0, 0, 0, 3, 0, 0, 0, 3, 0,
                                                 2, 0, 0, 0, 3, 1, 0, 0, 1, 3, 0, 0, 0, 0,
                                                 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
                                                 0, 0, 0, 0, 0, 1, 3, 1, 0, 0, 0, 0, 0, 0,
                                                 0, 0, 0, 0, 0, 0, 0, 0, 2, 0, 0, 0, 0, 0,
                                                 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 2, 3, 1, 3,
                                                 0, 0, 1, 3, 0, 0, 0, 0, 0, 0, 0, 1, 0, 0,
                                                 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
                                                 2, 3, 0, 1, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0,
                                                 0, 0, 3, 3, 0, 1, 0, 0, 0, 0, 2, 2, 0, 0,
                                                 2, 0, 0, 0};

    size_t const nrows = height + 2;
    size_t const ncols = width + 2;

    // Create skeleton and cleaned_skeleton with border
    std::vector<uint8_t> skeleton(nrows * ncols, 0);
    std::vector<uint8_t> cleaned_skeleton(nrows * ncols, 0);

    // Copy image into skeleton with border, normalizing to binary 0/1 values
    for (size_t row = 0; row < height; ++row) {
        for (size_t col = 0; col < width; ++col) {
            skeleton[(row + 1) * ncols + (col + 1)] = (image[row * width + col] > 0) ? 1 : 0;
        }
    }

    std::memcpy(cleaned_skeleton.data(), skeleton.data(), nrows * ncols);


    bool pixel_removed = true;

    while (pixel_removed) {
        pixel_removed = false;

        for (int pass_num = 0; pass_num < 2; ++pass_num) {
            bool const first_pass = (pass_num == 0);

            for (size_t row = 1; row < nrows - 1; ++row) {
                for (size_t col = 1; col < ncols - 1; ++col) {
                    if (skeleton[row * ncols + col]) {
                        uint8_t const neighbors = lut[static_cast<size_t>(skeleton[(row - 1) * ncols + (col - 1)] +
                                                      2 * skeleton[(row - 1) * ncols + col] +
                                                      4 * skeleton[(row - 1) * ncols + (col + 1)] +
                                                      8 * skeleton[row * ncols + (col + 1)] +
                                                      16 * skeleton[(row + 1) * ncols + (col + 1)] +
                                                      32 * skeleton[(row + 1) * ncols + col] +
                                                      64 * skeleton[(row + 1) * ncols + (col - 1)] +
                                                      128 * skeleton[row * ncols + (col - 1)])];

                        if (neighbors == 0) {
                            continue;
                        } else if ((neighbors == 3) || (neighbors == 1 && first_pass) || (neighbors == 2 && !first_pass)) {
                            cleaned_skeleton[row * ncols + col] = 0;
                            pixel_removed = true;
                        }
                    }
                }
            }

            // Update skeleton with cleaned_skeleton
            std::memcpy(skeleton.data(), cleaned_skeleton.data(), nrows * ncols);
        }
    }

    // Remove border and return the result
    std::vector<uint8_t> result(height * width, 0);
    for (size_t row = 0; row < height; ++row) {
        for (size_t col = 0; col < width; ++col) {
            result[row * width + col] = skeleton[(row + 1) * ncols + (col + 1)];
        }
    }

    return result;
}

Image fast_skeletonize(Image const & input_image) {
    // Delegate to the existing function
    auto result_data = fast_skeletonize(input_image.data,
                                        static_cast<size_t>(input_image.size.height),
                                        static_cast<size_t>(input_image.size.width));

    // Return as Image struct
    return {std::move(result_data), input_image.size};
}
