#include <catch2/catch_test_macros.hpp>

#include "CoreMath/non_finite_rows.hpp"

#include <cmath>
#include <limits>

TEST_CASE("non_finite_rows: hasNonFiniteRows on clean fmat", "[CoreMath][non_finite_rows]") {
    arma::fmat mat(3, 2);
    mat.fill(1.0f);

    CHECK_FALSE(hasNonFiniteRows(mat));
}

TEST_CASE("non_finite_rows: hasNonFiniteRows detects NaN and Inf", "[CoreMath][non_finite_rows]") {
    arma::fmat mat(2, 2);
    mat(0, 0) = 1.0f;
    mat(0, 1) = 2.0f;
    mat(1, 0) = std::numeric_limits<float>::quiet_NaN();
    mat(1, 1) = 4.0f;

    CHECK(hasNonFiniteRows(mat));

    mat(1, 0) = 5.0f;
    mat(1, 1) = std::numeric_limits<float>::infinity();
    CHECK(hasNonFiniteRows(mat));
}

TEST_CASE("non_finite_rows: filterNonFiniteRows all finite fast path", "[CoreMath][non_finite_rows]") {
    arma::mat mat(3, 2);
    mat(0, 0) = 1.0;
    mat(0, 1) = 2.0;
    mat(1, 0) = 3.0;
    mat(1, 1) = 4.0;
    mat(2, 0) = 5.0;
    mat(2, 1) = 6.0;

    auto const result = filterNonFiniteRows(mat);

    CHECK(result.rows_dropped == 0);
    REQUIRE(result.valid_row_indices.size() == 3);
    CHECK(result.valid_row_indices[0] == 0);
    CHECK(result.valid_row_indices[1] == 1);
    CHECK(result.valid_row_indices[2] == 2);
    CHECK(arma::approx_equal(result.clean_matrix, mat, "absdiff", 0.0));
}

TEST_CASE("non_finite_rows: filterNonFiniteRows drops bad rows", "[CoreMath][non_finite_rows]") {
    arma::mat mat(4, 2);
    mat.fill(1.0);
    mat(1, 0) = std::numeric_limits<double>::quiet_NaN();
    mat(3, 1) = std::numeric_limits<double>::infinity();

    auto const result = filterNonFiniteRows(mat);

    CHECK(result.rows_dropped == 2);
    REQUIRE(result.clean_matrix.n_rows == 2);
    REQUIRE(result.valid_row_indices.size() == 2);
    CHECK(result.valid_row_indices[0] == 0);
    CHECK(result.valid_row_indices[1] == 2);

    for (arma::uword r = 0; r < result.clean_matrix.n_rows; ++r) {
        for (arma::uword c = 0; c < result.clean_matrix.n_cols; ++c) {
            CHECK(std::isfinite(result.clean_matrix(r, c)));
        }
    }
}

TEST_CASE("non_finite_rows: rowHasNonFinite span helper", "[CoreMath][non_finite_rows]") {
    std::vector<float> const clean = {1.0f, 2.0f, 3.0f};
    CHECK_FALSE(rowHasNonFinite(clean));

    std::vector<float> const bad = {1.0f, std::numeric_limits<float>::quiet_NaN(), 3.0f};
    CHECK(rowHasNonFinite(bad));
}

TEST_CASE("non_finite_rows: rowHasNonFiniteAcrossColumns", "[CoreMath][non_finite_rows]") {
    std::vector<std::vector<float>> columns = {
            {1.0f, 2.0f, 3.0f},
            {4.0f, 5.0f, 6.0f},
    };

    CHECK_FALSE(rowHasNonFiniteAcrossColumns(columns, 1));

    columns[0][1] = std::numeric_limits<float>::quiet_NaN();
    CHECK(rowHasNonFiniteAcrossColumns(columns, 1));
}
