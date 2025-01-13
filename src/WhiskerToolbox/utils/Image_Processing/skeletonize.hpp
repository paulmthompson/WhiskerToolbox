#ifndef SKELETONIZE_HPP
#define SKELETONIZE_HPP

/*

The code below was adapted from scikit-image

Function: skimage/morphology/_skeletonize_various_cy.pyx:_skeletonize_loop
Copyright: 2003-2009 Massachusetts Institute of Technology
           2009-2011 Broad Institute
           2003 Lee Kamentsky
License: BSD-3-Clause
*/

#include <array>
#include <cstring>
#include <stdint.h>
#include <vector>

std::vector<uint8_t> fast_skeletonize(const std::vector<uint8_t>& image, int height, int width) {
    // Look up table
    const std::array<uint8_t, 256> lut = {0, 0, 0, 1, 0, 0, 1, 3, 0, 0, 3, 1, 1, 0,
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

    const int nrows = height + 2;
    const int ncols = width + 2;

    // Create skeleton and cleaned_skeleton with border
    std::vector<uint8_t> skeleton(nrows * ncols, 0);
    std::vector<uint8_t> cleaned_skeleton(nrows * ncols, 0);

    // Copy image into skeleton with border
    for (int row = 0; row < height; ++row) {
        std::memcpy(&skeleton[(row + 1) * ncols + 1], &image[row * width], width);
    }

    std::memcpy(cleaned_skeleton.data(), skeleton.data(), nrows * ncols);


    bool pixel_removed = true;

    while (pixel_removed) {
        pixel_removed = false;

        for (int pass_num = 0; pass_num < 2; ++pass_num) {
            bool first_pass = (pass_num == 0);

            for (int row = 1; row < nrows - 1; ++row) {
                for (int col = 1; col < ncols - 1; ++col) {
                    if (skeleton[row * ncols + col]) {
                        uint8_t neighbors = lut[
                            skeleton[(row - 1) * ncols + (col - 1)] +
                            2 * skeleton[(row - 1) * ncols + col] +
                            4 * skeleton[(row - 1) * ncols + (col + 1)] +
                            8 * skeleton[row * ncols + (col + 1)] +
                            16 * skeleton[(row + 1) * ncols + (col + 1)] +
                            32 * skeleton[(row + 1) * ncols + col] +
                            64 * skeleton[(row + 1) * ncols + (col - 1)] +
                            128 * skeleton[row * ncols + (col - 1)]
                        ];

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
    for (int row = 0; row < height; ++row) {
        std::memcpy(&result[row * width], &skeleton[(row + 1) * ncols + 1], width);
    }

    return result;
}


#endif // SKELETONIZE_HPP
