/**
 * @file whiskertoolbox_pch.hpp
 * @brief Precompiled header for the WhiskerToolbox project.
 *
 * Contains expensive STL and third-party headers that are widely included
 * across the codebase. Precompiling these reduces per-TU parse time
 * significantly (see docs/developer/build_time_analysis/ for measurements).
 *
 * Only add headers here that satisfy ALL of:
 *   1. Expensive to compile (>100 ms per include)
 *   2. Included in many translation units (>20)
 *   3. Rarely changed (stable API)
 */

#ifndef WHISKERTOOLBOX_PCH_HPP
#define WHISKERTOOLBOX_PCH_HPP

// ── Standard library containers ──────────────────────────────────────
#include <array>
#include <deque>
#include <map>
#include <set>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

// ── Standard library utilities ───────────────────────────────────────
#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <iterator>
#include <memory>
#include <numeric>
#include <optional>
#include <span>
#include <string_view>
#include <tuple>
#include <type_traits>
#include <utility>
#include <variant>

// ── Standard library I/O & formatting ────────────────────────────────
#include <format>
#include <sstream>

// ── Standard library concurrency ─────────────────────────────────────
#include <atomic>
#include <mutex>

// ── Widely-used third-party headers ──────────────────────────────────
//#include <nlohmann/json.hpp>
//#include <spdlog/spdlog.h>

#endif // WHISKERTOOLBOX_PCH_HPP
