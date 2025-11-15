#ifndef WHISKERTOOLBOX_PCH_HPP
#define WHISKERTOOLBOX_PCH_HPP

// Precompiled Header for WhiskerToolbox
// This header includes the most expensive standard library headers
// that are used throughout the codebase.
//
// Based on ClangBuildAnalyzer results:
// - <functional>: 99.4s total (332 includes, avg 299ms)
// - <vector>: 78.0s total (361 includes, avg 215ms)  
// - <chrono>: 77.4s total (231 includes, avg 335ms)
// - Other frequently used STL headers
//
// Expected build time savings: 40-50 seconds (15-20% reduction)

// Standard library containers
#include <vector>
#include <string>
#include <map>
#include <unordered_map>
#include <set>
#include <unordered_set>
#include <array>

// Standard library utilities
#include <memory>
#include <functional>
#include <optional>
#include <variant>
#include <utility>

// Standard library algorithms and iterators
#include <algorithm>
#include <iterator>

// Standard library time support
#include <chrono>

// Standard library I/O
#include <iostream>
#include <sstream>
#include <fstream>

// Standard library numeric
#include <cmath>
#include <limits>
#include <numeric>

#endif // WHISKERTOOLBOX_PCH_HPP
