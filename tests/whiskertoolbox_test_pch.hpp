/**
 * @file whiskertoolbox_test_pch.hpp
 * @brief Precompiled header for WhiskerToolbox test targets.
 *
 * Extends the main project PCH with Catch2 headers that appear in nearly
 * every test translation unit.
 */

#ifndef WHISKERTOOLBOX_TEST_PCH_HPP
#define WHISKERTOOLBOX_TEST_PCH_HPP

// ── Project PCH (STL + third-party) ─────────────────────────────────
#include "../src/whiskertoolbox_pch.hpp"

// ── Catch2 headers ──────────────────────────────────────────────────
/*
CHECK and REQUIRE macros will require special attention
if we use a braced initialize instead of parentheses, e.g. REQUIRE(x == 42) vs REQUIRE({x} == 42).
*/
#include <catch2/catch_test_macros.hpp>                    // 174 files
#include <catch2/matchers/catch_matchers_floating_point.hpp> // 89 files
#include <catch2/catch_approx.hpp>                         // 16 files

#endif // WHISKERTOOLBOX_TEST_PCH_HPP
